#ifdef INFINIT_WINDOWS
# include <winsock2.h>
#endif

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <elle/finally.hh>
#include <elle/os/environ.hh>
#include <elle/test.hh>

#include <reactor/Barrier.hh>
#include <reactor/Scope.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/network/fingerprinted-socket.hh>
#include <reactor/network/ssl-server.hh>
#include <reactor/network/ssl-socket.hh>
#include <reactor/network/tcp-server.hh>
#include <reactor/thread.hh>

ELLE_LOG_COMPONENT("reactor.network.SSL.test");

using reactor::network::FingerprintedSocket;
using reactor::network::SSLCertificate;
using reactor::network::Socket;
using reactor::network::SSLSocket;
using reactor::network::SSLServer;

extern const std::vector<unsigned char> fingerprint;
extern const std::vector<char> server_cert;
extern const std::vector<char> server_key;
extern const std::vector<char> server_dh1024;

static
std::unique_ptr<SSLCertificate>
load_certificate()
{
  boost::filesystem::path tmp;
  while (true)
  {
    tmp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
    if (boost::filesystem::create_directory(tmp))
      break;
  }
  elle::SafeFinally remove_tmp([tmp] { boost::filesystem::remove_all(tmp); } );
  auto cert = tmp / "server-cert.pem";
  auto key = tmp / "server-key.pem";
  auto dh1024 = tmp / "dh1024.pem";
  {
    boost::filesystem::ofstream cert_f(cert, std::ios::binary);
    cert_f.write(server_cert.data(), server_cert.size());
  }
  {
    boost::filesystem::ofstream key_f(key, std::ios::binary);
    key_f.write(server_key.data(), server_key.size());
  }
  {
    boost::filesystem::ofstream dh1024_f(dh1024, std::ios::binary);
    dh1024_f.write(server_dh1024.data(), server_dh1024.size());
  }
  return elle::make_unique<SSLCertificate>(cert.string(),
                                           key.string(),
                                           dh1024.string());
}

ELLE_TEST_SCHEDULED(transfer)
{
  reactor::Barrier listening;
  int port = 0;
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background(
      "server",
      [&]
      {
        ELLE_DEBUG("read certificate from disk");
        auto certificate = load_certificate();
        SSLServer server(std::move(certificate));
        server.listen(0);
        port = server.port();
        listening.open();
        std::unique_ptr<Socket> socket(server.accept());
        static char servdata[5] = { 0 };
        socket->std::iostream::read(servdata, 4);
        BOOST_CHECK(std::string(servdata) == std::string("lulz"));
        socket->write(std::string("lol"));
        socket->write(std::string("lulz"));
      });
    scope.run_background(
      "client",
      [&]
      {
        SSLCertificate certificate;
        reactor::wait(listening);
        auto endpoint = reactor::network::resolve_tcp(
          "127.0.0.1",
          boost::lexical_cast<std::string>(port));
        FingerprintedSocket socket(endpoint,
                                   fingerprint);
        socket.write(std::string("lulz"));
        static char clientdata[5] = { 0 };
        socket.std::iostream::read(clientdata, 3);
        BOOST_CHECK(std::string(clientdata) == std::string("lol"));
        socket.std::iostream::read(clientdata, 4);
        BOOST_CHECK(std::string(clientdata) == std::string("lulz"));
      });
    reactor::wait(scope);
  };
}

class Sniffer
{
public:
  Sniffer(std::string const& secret):
    _thread("sniffer", std::bind(&Sniffer::_run, this)),
    _secret(secret),
    _server()
  {
    this->_server.listen();
  }

  ~Sniffer()
  {
    this->_thread.terminate_now();
  }

private:
  void
  _run()
  {
    using reactor::network::Socket;
    std::unique_ptr<Socket> a(this->_server.accept());
    std::unique_ptr<Socket> b(this->_server.accept());

    elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
    {
      auto route = [this] (Socket& from, Socket& to)
        {
          try
          {
            while (true)
            {
              char data[BUFSIZ];
              auto read = from.read_some(reactor::network::Buffer(data, BUFSIZ));
              for (unsigned i = 0; i < read - this->_secret.size(); ++i)
              {
                BOOST_CHECK_NE(std::string(data + i, this->_secret.size()),
                               this->_secret);
              }
              to.write(elle::WeakBuffer(data, read));
            }
          }
          catch (reactor::network::ConnectionClosed const&)
          { /* Nothing */ }
        };
      scope.run_background(
        elle::sprintf("%s: a to b", *this),
        std::bind(route, std::ref(*a), std::ref(*b)));
      scope.run_background(
        elle::sprintf("%s: b to a", *this),
        std::bind(route, std::ref(*b), std::ref(*a)));
      reactor::wait(scope);
    };
  }

  ELLE_ATTRIBUTE(reactor::Thread, thread);
  ELLE_ATTRIBUTE_R(std::string, secret);
  ELLE_ATTRIBUTE_R(reactor::network::TCPServer, server);
};

ELLE_TEST_SCHEDULED(encryption)
{
  Sniffer sniffer("lulz");

  auto certificate = load_certificate();
  auto port = boost::lexical_cast<std::string>(sniffer.server().port());

  std::string question = "lulz why do that ?";
  std::string answer = "well for the lulz !";

  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background(
      "question",
      [&]
      {
        SSLSocket a("127.0.0.1", port);
        ELLE_LOG("send question")
          a.write(elle::ConstWeakBuffer(question));
        ELLE_LOG("read answer")
          BOOST_CHECK_EQUAL(a.read(answer.size()).string(), answer);
      });
    scope.run_background(
      "answer",
      [&]
      {
        SSLSocket b("127.0.0.1", port, *certificate);
        ELLE_LOG("read question")
          BOOST_CHECK_EQUAL(b.read(question.size()).string(), question);
        ELLE_LOG("send answer")
          b.write(elle::ConstWeakBuffer(answer));
      });
    reactor::wait(scope);
  };
}

ELLE_TEST_SCHEDULED(handshake_timeout)
{
  reactor::Barrier listening;
  reactor::Barrier timed_out;
  int port = 0;
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background(
      "server",
      [&]
      {
        reactor::network::TCPServer server;
        server.listen();
        port = server.port();
        listening.open();
        std::unique_ptr<reactor::network::Socket> socket(server.accept());
        reactor::wait(timed_out);
      });
    scope.run_background(
      "client",
      [&]
      {
        reactor::wait(listening);
        BOOST_CHECK_THROW(
          SSLSocket("127.0.0.1",
                    boost::lexical_cast<std::string>(port),
                    1_sec),
          reactor::network::TimeOut);
        timed_out.open();
      });
    reactor::wait(scope);
  };
}

ELLE_TEST_SCHEDULED(connection_closed)
{
  reactor::Barrier listening;
  reactor::Barrier read;
  int port = 0;
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background(
      "server",
      [&]
      {
        reactor::network::SSLServer server(load_certificate());
        server.listen();
        port = server.port();
        listening.open();
        std::unique_ptr<reactor::network::Socket> socket(server.accept());
        socket->write(elle::ConstWeakBuffer("data"));
        reactor::wait(read);
      });
    scope.run_background(
      "client",
      [&]
      {
        reactor::wait(listening);
        SSLSocket s("127.0.0.1", boost::lexical_cast<std::string>(port));
        BOOST_CHECK_EQUAL(s.read(4).string(), "data");
        read.open();
        BOOST_CHECK_THROW(s.read(4), reactor::network::ConnectionClosed);
      });
    reactor::wait(scope);
  };
}

// Check a long or infinite handshake does not block subsequent ones.
ELLE_TEST_SCHEDULED(handshake_stuck)
{
  reactor::Barrier closed;
  reactor::Barrier listening;
  reactor::Barrier stuck;
  bool not_stuck = false;
  int port = 0;
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background(
      "server",
      [&]
      {
        reactor::network::SSLServer server(load_certificate(), 500_ms);
        server.listen();
        port = server.port();
        listening.open();
        std::unique_ptr<reactor::network::Socket> socket(server.accept());
        socket->write(elle::ConstWeakBuffer("not stuck"));
        reactor::wait(closed);
      });
    scope.run_background(
      "stuck client",
      [&]
      {
        reactor::wait(listening);
        reactor::network::TCPSocket s("127.0.0.1",
                                      boost::lexical_cast<std::string>(port));
        stuck.open();
        ELLE_LOG("HELLO");
        BOOST_CHECK_THROW(s.read(4), reactor::network::ConnectionClosed);
        ELLE_LOG("BYE");
        closed.open();
        BOOST_CHECK(not_stuck);
      });
    scope.run_background(
      "client",
      [&]
      {
        reactor::wait(listening);
        reactor::wait(stuck);
        SSLSocket s("127.0.0.1", boost::lexical_cast<std::string>(port));
        BOOST_CHECK_EQUAL(s.read(9).string(), "not stuck");
        not_stuck = true;
      });
    reactor::wait(scope);
  };

}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(transfer), 0, 10);
  suite.add(BOOST_TEST_CASE(handshake_timeout), 0, 10);
  suite.add(BOOST_TEST_CASE(encryption), 0, 10);
  suite.add(BOOST_TEST_CASE(connection_closed), 0, 10);
  suite.add(BOOST_TEST_CASE(handshake_stuck), 0, 3);
}

const std::vector<unsigned char> fingerprint =
{
  0x66, 0x84, 0x68, 0xEB, 0xBE, 0x83, 0xA0, 0x5C, 0x6A, 0x32,
  0xAD, 0xD2, 0x58, 0x62, 0x01, 0x31, 0x79, 0x96, 0x78, 0xB8
};

const std::vector<char> server_cert =
{
  0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69, 0x63, 0x61, 0x74, 0x65, 0x3a,
  0x0a, 0x20, 0x20, 0x20, 0x20, 0x44, 0x61, 0x74, 0x61, 0x3a, 0x0a, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x56, 0x65, 0x72, 0x73, 0x69,
  0x6f, 0x6e, 0x3a, 0x20, 0x33, 0x20, 0x28, 0x30, 0x78, 0x32, 0x29, 0x0a,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x53, 0x65, 0x72, 0x69,
  0x61, 0x6c, 0x20, 0x4e, 0x75, 0x6d, 0x62, 0x65, 0x72, 0x3a, 0x20, 0x34,
  0x30, 0x39, 0x36, 0x20, 0x28, 0x30, 0x78, 0x31, 0x30, 0x30, 0x30, 0x29,
  0x0a, 0x20, 0x20, 0x20, 0x20, 0x53, 0x69, 0x67, 0x6e, 0x61, 0x74, 0x75,
  0x72, 0x65, 0x20, 0x41, 0x6c, 0x67, 0x6f, 0x72, 0x69, 0x74, 0x68, 0x6d,
  0x3a, 0x20, 0x73, 0x68, 0x61, 0x31, 0x57, 0x69, 0x74, 0x68, 0x52, 0x53,
  0x41, 0x45, 0x6e, 0x63, 0x72, 0x79, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x0a,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x49, 0x73, 0x73, 0x75,
  0x65, 0x72, 0x3a, 0x20, 0x43, 0x3d, 0x46, 0x52, 0x2c, 0x20, 0x53, 0x54,
  0x3d, 0x49, 0x6c, 0x65, 0x2d, 0x64, 0x65, 0x2d, 0x46, 0x72, 0x61, 0x6e,
  0x63, 0x65, 0x2c, 0x20, 0x4c, 0x3d, 0x50, 0x61, 0x72, 0x69, 0x73, 0x2c,
  0x20, 0x4f, 0x3d, 0x49, 0x6e, 0x66, 0x69, 0x6e, 0x69, 0x74, 0x2e, 0x69,
  0x6f, 0x2c, 0x20, 0x4f, 0x55, 0x3d, 0x44, 0x65, 0x76, 0x65, 0x6c, 0x6f,
  0x70, 0x6d, 0x65, 0x6e, 0x74, 0x2c, 0x20, 0x43, 0x4e, 0x3d, 0x4c, 0x6f,
  0x75, 0x69, 0x73, 0x20, 0x46, 0x45, 0x55, 0x56, 0x52, 0x49, 0x45, 0x52,
  0x2f, 0x65, 0x6d, 0x61, 0x69, 0x6c, 0x41, 0x64, 0x64, 0x72, 0x65, 0x73,
  0x73, 0x3d, 0x6c, 0x6f, 0x75, 0x69, 0x73, 0x2e, 0x66, 0x65, 0x75, 0x76,
  0x72, 0x69, 0x65, 0x72, 0x40, 0x69, 0x6e, 0x66, 0x69, 0x6e, 0x69, 0x74,
  0x2e, 0x69, 0x6f, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x56, 0x61, 0x6c, 0x69, 0x64, 0x69, 0x74, 0x79, 0x0a, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x4e, 0x6f, 0x74,
  0x20, 0x42, 0x65, 0x66, 0x6f, 0x72, 0x65, 0x3a, 0x20, 0x44, 0x65, 0x63,
  0x20, 0x20, 0x35, 0x20, 0x31, 0x30, 0x3a, 0x35, 0x39, 0x3a, 0x34, 0x38,
  0x20, 0x32, 0x30, 0x31, 0x33, 0x20, 0x47, 0x4d, 0x54, 0x0a, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x4e, 0x6f,
  0x74, 0x20, 0x41, 0x66, 0x74, 0x65, 0x72, 0x20, 0x3a, 0x20, 0x44, 0x65,
  0x63, 0x20, 0x20, 0x35, 0x20, 0x31, 0x30, 0x3a, 0x35, 0x39, 0x3a, 0x34,
  0x38, 0x20, 0x32, 0x30, 0x31, 0x34, 0x20, 0x47, 0x4d, 0x54, 0x0a, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x53, 0x75, 0x62, 0x6a, 0x65,
  0x63, 0x74, 0x3a, 0x20, 0x43, 0x3d, 0x46, 0x52, 0x2c, 0x20, 0x53, 0x54,
  0x3d, 0x49, 0x6c, 0x65, 0x2d, 0x64, 0x65, 0x2d, 0x46, 0x72, 0x61, 0x6e,
  0x63, 0x65, 0x2c, 0x20, 0x4f, 0x3d, 0x49, 0x6e, 0x66, 0x69, 0x6e, 0x69,
  0x74, 0x2e, 0x69, 0x6f, 0x2c, 0x20, 0x4f, 0x55, 0x3d, 0x44, 0x65, 0x76,
  0x65, 0x6c, 0x6f, 0x70, 0x6d, 0x65, 0x6e, 0x74, 0x2c, 0x20, 0x43, 0x4e,
  0x3d, 0x4c, 0x6f, 0x75, 0x69, 0x73, 0x20, 0x46, 0x45, 0x55, 0x56, 0x52,
  0x49, 0x45, 0x52, 0x2f, 0x65, 0x6d, 0x61, 0x69, 0x6c, 0x41, 0x64, 0x64,
  0x72, 0x65, 0x73, 0x73, 0x3d, 0x6c, 0x6f, 0x75, 0x69, 0x73, 0x2e, 0x66,
  0x65, 0x75, 0x76, 0x72, 0x69, 0x65, 0x72, 0x40, 0x69, 0x6e, 0x66, 0x69,
  0x6e, 0x69, 0x74, 0x2e, 0x69, 0x6f, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x53, 0x75, 0x62, 0x6a, 0x65, 0x63, 0x74, 0x20, 0x50,
  0x75, 0x62, 0x6c, 0x69, 0x63, 0x20, 0x4b, 0x65, 0x79, 0x20, 0x49, 0x6e,
  0x66, 0x6f, 0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x50, 0x75, 0x62, 0x6c, 0x69, 0x63, 0x20, 0x4b,
  0x65, 0x79, 0x20, 0x41, 0x6c, 0x67, 0x6f, 0x72, 0x69, 0x74, 0x68, 0x6d,
  0x3a, 0x20, 0x72, 0x73, 0x61, 0x45, 0x6e, 0x63, 0x72, 0x79, 0x70, 0x74,
  0x69, 0x6f, 0x6e, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x50, 0x75, 0x62, 0x6c,
  0x69, 0x63, 0x2d, 0x4b, 0x65, 0x79, 0x3a, 0x20, 0x28, 0x31, 0x30, 0x32,
  0x34, 0x20, 0x62, 0x69, 0x74, 0x29, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x4d,
  0x6f, 0x64, 0x75, 0x6c, 0x75, 0x73, 0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x30, 0x30, 0x3a, 0x64, 0x30, 0x3a, 0x66, 0x66,
  0x3a, 0x61, 0x30, 0x3a, 0x34, 0x34, 0x3a, 0x63, 0x66, 0x3a, 0x65, 0x35,
  0x3a, 0x30, 0x65, 0x3a, 0x63, 0x34, 0x3a, 0x62, 0x66, 0x3a, 0x32, 0x32,
  0x3a, 0x35, 0x33, 0x3a, 0x64, 0x38, 0x3a, 0x30, 0x65, 0x3a, 0x36, 0x39,
  0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x35, 0x61,
  0x3a, 0x31, 0x61, 0x3a, 0x33, 0x65, 0x3a, 0x31, 0x64, 0x3a, 0x37, 0x38,
  0x3a, 0x63, 0x39, 0x3a, 0x35, 0x32, 0x3a, 0x38, 0x35, 0x3a, 0x38, 0x30,
  0x3a, 0x31, 0x63, 0x3a, 0x65, 0x35, 0x3a, 0x65, 0x64, 0x3a, 0x66, 0x35,
  0x3a, 0x32, 0x62, 0x3a, 0x30, 0x37, 0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x66, 0x63, 0x3a, 0x63, 0x37, 0x3a, 0x32, 0x62,
  0x3a, 0x63, 0x61, 0x3a, 0x31, 0x38, 0x3a, 0x37, 0x35, 0x3a, 0x66, 0x37,
  0x3a, 0x39, 0x31, 0x3a, 0x35, 0x61, 0x3a, 0x39, 0x32, 0x3a, 0x37, 0x39,
  0x3a, 0x35, 0x35, 0x3a, 0x66, 0x36, 0x3a, 0x65, 0x39, 0x3a, 0x34, 0x32,
  0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x65, 0x30,
  0x3a, 0x65, 0x30, 0x3a, 0x61, 0x33, 0x3a, 0x64, 0x66, 0x3a, 0x66, 0x62,
  0x3a, 0x33, 0x65, 0x3a, 0x62, 0x34, 0x3a, 0x34, 0x32, 0x3a, 0x64, 0x30,
  0x3a, 0x34, 0x62, 0x3a, 0x66, 0x35, 0x3a, 0x65, 0x62, 0x3a, 0x30, 0x35,
  0x3a, 0x34, 0x34, 0x3a, 0x33, 0x32, 0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x32, 0x63, 0x3a, 0x34, 0x61, 0x3a, 0x38, 0x35,
  0x3a, 0x64, 0x30, 0x3a, 0x65, 0x31, 0x3a, 0x61, 0x33, 0x3a, 0x36, 0x63,
  0x3a, 0x61, 0x61, 0x3a, 0x66, 0x30, 0x3a, 0x30, 0x31, 0x3a, 0x66, 0x37,
  0x3a, 0x31, 0x32, 0x3a, 0x63, 0x34, 0x3a, 0x30, 0x65, 0x3a, 0x66, 0x66,
  0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x30, 0x30,
  0x3a, 0x61, 0x31, 0x3a, 0x37, 0x34, 0x3a, 0x64, 0x32, 0x3a, 0x36, 0x35,
  0x3a, 0x32, 0x30, 0x3a, 0x64, 0x62, 0x3a, 0x35, 0x61, 0x3a, 0x30, 0x33,
  0x3a, 0x37, 0x38, 0x3a, 0x63, 0x37, 0x3a, 0x35, 0x34, 0x3a, 0x61, 0x31,
  0x3a, 0x62, 0x63, 0x3a, 0x64, 0x37, 0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x38, 0x33, 0x3a, 0x66, 0x66, 0x3a, 0x39, 0x37,
  0x3a, 0x37, 0x38, 0x3a, 0x62, 0x64, 0x3a, 0x31, 0x34, 0x3a, 0x64, 0x36,
  0x3a, 0x35, 0x31, 0x3a, 0x33, 0x35, 0x3a, 0x62, 0x61, 0x3a, 0x36, 0x30,
  0x3a, 0x31, 0x62, 0x3a, 0x66, 0x66, 0x3a, 0x39, 0x34, 0x3a, 0x62, 0x66,
  0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x37, 0x65,
  0x3a, 0x34, 0x63, 0x3a, 0x34, 0x34, 0x3a, 0x61, 0x62, 0x3a, 0x63, 0x63,
  0x3a, 0x62, 0x36, 0x3a, 0x39, 0x33, 0x3a, 0x30, 0x63, 0x3a, 0x36, 0x61,
  0x3a, 0x35, 0x66, 0x3a, 0x38, 0x36, 0x3a, 0x38, 0x65, 0x3a, 0x32, 0x38,
  0x3a, 0x36, 0x62, 0x3a, 0x39, 0x31, 0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x65, 0x64, 0x3a, 0x32, 0x38, 0x3a, 0x62, 0x61,
  0x3a, 0x34, 0x61, 0x3a, 0x36, 0x66, 0x3a, 0x34, 0x66, 0x3a, 0x33, 0x33,
  0x3a, 0x33, 0x66, 0x3a, 0x34, 0x35, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x45,
  0x78, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x3a, 0x20, 0x36, 0x35, 0x35,
  0x33, 0x37, 0x20, 0x28, 0x30, 0x78, 0x31, 0x30, 0x30, 0x30, 0x31, 0x29,
  0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x58, 0x35, 0x30,
  0x39, 0x76, 0x33, 0x20, 0x65, 0x78, 0x74, 0x65, 0x6e, 0x73, 0x69, 0x6f,
  0x6e, 0x73, 0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x58, 0x35, 0x30, 0x39, 0x76, 0x33, 0x20, 0x42,
  0x61, 0x73, 0x69, 0x63, 0x20, 0x43, 0x6f, 0x6e, 0x73, 0x74, 0x72, 0x61,
  0x69, 0x6e, 0x74, 0x73, 0x3a, 0x20, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x43,
  0x41, 0x3a, 0x46, 0x41, 0x4c, 0x53, 0x45, 0x0a, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x4e, 0x65, 0x74, 0x73,
  0x63, 0x61, 0x70, 0x65, 0x20, 0x43, 0x6f, 0x6d, 0x6d, 0x65, 0x6e, 0x74,
  0x3a, 0x20, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x4f, 0x70, 0x65, 0x6e, 0x53,
  0x53, 0x4c, 0x20, 0x47, 0x65, 0x6e, 0x65, 0x72, 0x61, 0x74, 0x65, 0x64,
  0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69, 0x63, 0x61, 0x74, 0x65,
  0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x58, 0x35, 0x30, 0x39, 0x76, 0x33, 0x20, 0x53, 0x75, 0x62, 0x6a,
  0x65, 0x63, 0x74, 0x20, 0x4b, 0x65, 0x79, 0x20, 0x49, 0x64, 0x65, 0x6e,
  0x74, 0x69, 0x66, 0x69, 0x65, 0x72, 0x3a, 0x20, 0x0a, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x44, 0x35, 0x3a, 0x35, 0x44, 0x3a, 0x43, 0x36, 0x3a, 0x32, 0x43,
  0x3a, 0x41, 0x42, 0x3a, 0x31, 0x46, 0x3a, 0x42, 0x44, 0x3a, 0x32, 0x41,
  0x3a, 0x35, 0x42, 0x3a, 0x45, 0x38, 0x3a, 0x34, 0x46, 0x3a, 0x43, 0x36,
  0x3a, 0x36, 0x38, 0x3a, 0x42, 0x38, 0x3a, 0x42, 0x42, 0x3a, 0x33, 0x32,
  0x3a, 0x38, 0x37, 0x3a, 0x44, 0x34, 0x3a, 0x31, 0x44, 0x3a, 0x45, 0x35,
  0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x58, 0x35, 0x30, 0x39, 0x76, 0x33, 0x20, 0x41, 0x75, 0x74, 0x68,
  0x6f, 0x72, 0x69, 0x74, 0x79, 0x20, 0x4b, 0x65, 0x79, 0x20, 0x49, 0x64,
  0x65, 0x6e, 0x74, 0x69, 0x66, 0x69, 0x65, 0x72, 0x3a, 0x20, 0x0a, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x6b, 0x65, 0x79, 0x69, 0x64, 0x3a, 0x33, 0x37, 0x3a,
  0x38, 0x38, 0x3a, 0x37, 0x35, 0x3a, 0x43, 0x33, 0x3a, 0x34, 0x45, 0x3a,
  0x34, 0x42, 0x3a, 0x31, 0x33, 0x3a, 0x46, 0x33, 0x3a, 0x30, 0x32, 0x3a,
  0x37, 0x46, 0x3a, 0x30, 0x43, 0x3a, 0x42, 0x44, 0x3a, 0x44, 0x30, 0x3a,
  0x39, 0x43, 0x3a, 0x38, 0x35, 0x3a, 0x34, 0x36, 0x3a, 0x46, 0x39, 0x3a,
  0x42, 0x46, 0x3a, 0x34, 0x31, 0x3a, 0x44, 0x46, 0x0a, 0x0a, 0x20, 0x20,
  0x20, 0x20, 0x53, 0x69, 0x67, 0x6e, 0x61, 0x74, 0x75, 0x72, 0x65, 0x20,
  0x41, 0x6c, 0x67, 0x6f, 0x72, 0x69, 0x74, 0x68, 0x6d, 0x3a, 0x20, 0x73,
  0x68, 0x61, 0x31, 0x57, 0x69, 0x74, 0x68, 0x52, 0x53, 0x41, 0x45, 0x6e,
  0x63, 0x72, 0x79, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x0a, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x32, 0x61, 0x3a, 0x36, 0x35, 0x3a,
  0x63, 0x39, 0x3a, 0x33, 0x31, 0x3a, 0x66, 0x39, 0x3a, 0x65, 0x66, 0x3a,
  0x62, 0x35, 0x3a, 0x37, 0x37, 0x3a, 0x65, 0x37, 0x3a, 0x37, 0x36, 0x3a,
  0x66, 0x61, 0x3a, 0x30, 0x63, 0x3a, 0x34, 0x39, 0x3a, 0x64, 0x61, 0x3a,
  0x33, 0x33, 0x3a, 0x36, 0x33, 0x3a, 0x61, 0x38, 0x3a, 0x35, 0x38, 0x3a,
  0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38, 0x33,
  0x3a, 0x38, 0x31, 0x3a, 0x64, 0x61, 0x3a, 0x64, 0x37, 0x3a, 0x37, 0x32,
  0x3a, 0x36, 0x35, 0x3a, 0x66, 0x39, 0x3a, 0x39, 0x63, 0x3a, 0x30, 0x36,
  0x3a, 0x65, 0x31, 0x3a, 0x39, 0x36, 0x3a, 0x36, 0x36, 0x3a, 0x66, 0x63,
  0x3a, 0x38, 0x38, 0x3a, 0x61, 0x38, 0x3a, 0x63, 0x34, 0x3a, 0x31, 0x63,
  0x3a, 0x38, 0x66, 0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x62, 0x65, 0x3a, 0x36, 0x36, 0x3a, 0x39, 0x38, 0x3a, 0x33,
  0x64, 0x3a, 0x64, 0x62, 0x3a, 0x61, 0x65, 0x3a, 0x36, 0x32, 0x3a, 0x63,
  0x33, 0x3a, 0x66, 0x63, 0x3a, 0x37, 0x32, 0x3a, 0x66, 0x63, 0x3a, 0x32,
  0x62, 0x3a, 0x64, 0x37, 0x3a, 0x36, 0x63, 0x3a, 0x39, 0x37, 0x3a, 0x30,
  0x65, 0x3a, 0x31, 0x63, 0x3a, 0x35, 0x32, 0x3a, 0x0a, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x66, 0x39, 0x3a, 0x63, 0x63, 0x3a,
  0x66, 0x64, 0x3a, 0x65, 0x35, 0x3a, 0x66, 0x32, 0x3a, 0x65, 0x39, 0x3a,
  0x62, 0x36, 0x3a, 0x65, 0x31, 0x3a, 0x31, 0x38, 0x3a, 0x31, 0x30, 0x3a,
  0x33, 0x32, 0x3a, 0x36, 0x63, 0x3a, 0x31, 0x35, 0x3a, 0x38, 0x66, 0x3a,
  0x36, 0x63, 0x3a, 0x61, 0x34, 0x3a, 0x32, 0x64, 0x3a, 0x64, 0x35, 0x3a,
  0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x63, 0x31,
  0x3a, 0x35, 0x39, 0x3a, 0x38, 0x31, 0x3a, 0x39, 0x65, 0x3a, 0x37, 0x34,
  0x3a, 0x62, 0x32, 0x3a, 0x66, 0x35, 0x3a, 0x64, 0x35, 0x3a, 0x34, 0x37,
  0x3a, 0x38, 0x32, 0x3a, 0x34, 0x32, 0x3a, 0x32, 0x61, 0x3a, 0x31, 0x36,
  0x3a, 0x35, 0x32, 0x3a, 0x38, 0x32, 0x3a, 0x30, 0x61, 0x3a, 0x62, 0x63,
  0x3a, 0x32, 0x61, 0x3a, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x37, 0x33, 0x3a, 0x30, 0x39, 0x3a, 0x66, 0x65, 0x3a, 0x38,
  0x63, 0x3a, 0x61, 0x36, 0x3a, 0x34, 0x30, 0x3a, 0x30, 0x63, 0x3a, 0x34,
  0x39, 0x3a, 0x30, 0x36, 0x3a, 0x66, 0x62, 0x3a, 0x31, 0x34, 0x3a, 0x66,
  0x62, 0x3a, 0x35, 0x38, 0x3a, 0x63, 0x30, 0x3a, 0x34, 0x66, 0x3a, 0x36,
  0x32, 0x3a, 0x33, 0x63, 0x3a, 0x65, 0x30, 0x3a, 0x0a, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x63, 0x61, 0x3a, 0x62, 0x36, 0x3a,
  0x63, 0x38, 0x3a, 0x36, 0x36, 0x3a, 0x38, 0x64, 0x3a, 0x37, 0x37, 0x3a,
  0x63, 0x30, 0x3a, 0x36, 0x62, 0x3a, 0x36, 0x61, 0x3a, 0x64, 0x32, 0x3a,
  0x37, 0x33, 0x3a, 0x39, 0x37, 0x3a, 0x61, 0x35, 0x3a, 0x33, 0x33, 0x3a,
  0x30, 0x63, 0x3a, 0x62, 0x33, 0x3a, 0x64, 0x33, 0x3a, 0x34, 0x62, 0x3a,
  0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x30, 0x39,
  0x3a, 0x39, 0x33, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47,
  0x49, 0x4e, 0x20, 0x43, 0x45, 0x52, 0x54, 0x49, 0x46, 0x49, 0x43, 0x41,
  0x54, 0x45, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a, 0x4d, 0x49, 0x49, 0x44,
  0x4b, 0x6a, 0x43, 0x43, 0x41, 0x70, 0x4f, 0x67, 0x41, 0x77, 0x49, 0x42,
  0x41, 0x67, 0x49, 0x43, 0x45, 0x41, 0x41, 0x77, 0x44, 0x51, 0x59, 0x4a,
  0x4b, 0x6f, 0x5a, 0x49, 0x68, 0x76, 0x63, 0x4e, 0x41, 0x51, 0x45, 0x46,
  0x42, 0x51, 0x41, 0x77, 0x67, 0x61, 0x4d, 0x78, 0x43, 0x7a, 0x41, 0x4a,
  0x42, 0x67, 0x4e, 0x56, 0x42, 0x41, 0x59, 0x54, 0x41, 0x6b, 0x5a, 0x53,
  0x0a, 0x4d, 0x52, 0x59, 0x77, 0x46, 0x41, 0x59, 0x44, 0x56, 0x51, 0x51,
  0x49, 0x44, 0x41, 0x31, 0x4a, 0x62, 0x47, 0x55, 0x74, 0x5a, 0x47, 0x55,
  0x74, 0x52, 0x6e, 0x4a, 0x68, 0x62, 0x6d, 0x4e, 0x6c, 0x4d, 0x51, 0x34,
  0x77, 0x44, 0x41, 0x59, 0x44, 0x56, 0x51, 0x51, 0x48, 0x44, 0x41, 0x56,
  0x51, 0x59, 0x58, 0x4a, 0x70, 0x63, 0x7a, 0x45, 0x54, 0x4d, 0x42, 0x45,
  0x47, 0x41, 0x31, 0x55, 0x45, 0x0a, 0x43, 0x67, 0x77, 0x4b, 0x53, 0x57,
  0x35, 0x6d, 0x61, 0x57, 0x35, 0x70, 0x64, 0x43, 0x35, 0x70, 0x62, 0x7a,
  0x45, 0x55, 0x4d, 0x42, 0x49, 0x47, 0x41, 0x31, 0x55, 0x45, 0x43, 0x77,
  0x77, 0x4c, 0x52, 0x47, 0x56, 0x32, 0x5a, 0x57, 0x78, 0x76, 0x63, 0x47,
  0x31, 0x6c, 0x62, 0x6e, 0x51, 0x78, 0x46, 0x7a, 0x41, 0x56, 0x42, 0x67,
  0x4e, 0x56, 0x42, 0x41, 0x4d, 0x4d, 0x44, 0x6b, 0x78, 0x76, 0x0a, 0x64,
  0x57, 0x6c, 0x7a, 0x49, 0x45, 0x5a, 0x46, 0x56, 0x56, 0x5a, 0x53, 0x53,
  0x55, 0x56, 0x53, 0x4d, 0x53, 0x67, 0x77, 0x4a, 0x67, 0x59, 0x4a, 0x4b,
  0x6f, 0x5a, 0x49, 0x68, 0x76, 0x63, 0x4e, 0x41, 0x51, 0x6b, 0x42, 0x46,
  0x68, 0x6c, 0x73, 0x62, 0x33, 0x56, 0x70, 0x63, 0x79, 0x35, 0x6d, 0x5a,
  0x58, 0x56, 0x32, 0x63, 0x6d, 0x6c, 0x6c, 0x63, 0x6b, 0x42, 0x70, 0x62,
  0x6d, 0x5a, 0x70, 0x0a, 0x62, 0x6d, 0x6c, 0x30, 0x4c, 0x6d, 0x6c, 0x76,
  0x4d, 0x42, 0x34, 0x58, 0x44, 0x54, 0x45, 0x7a, 0x4d, 0x54, 0x49, 0x77,
  0x4e, 0x54, 0x45, 0x77, 0x4e, 0x54, 0x6b, 0x30, 0x4f, 0x46, 0x6f, 0x58,
  0x44, 0x54, 0x45, 0x30, 0x4d, 0x54, 0x49, 0x77, 0x4e, 0x54, 0x45, 0x77,
  0x4e, 0x54, 0x6b, 0x30, 0x4f, 0x46, 0x6f, 0x77, 0x67, 0x5a, 0x4d, 0x78,
  0x43, 0x7a, 0x41, 0x4a, 0x42, 0x67, 0x4e, 0x56, 0x0a, 0x42, 0x41, 0x59,
  0x54, 0x41, 0x6b, 0x5a, 0x53, 0x4d, 0x52, 0x59, 0x77, 0x46, 0x41, 0x59,
  0x44, 0x56, 0x51, 0x51, 0x49, 0x44, 0x41, 0x31, 0x4a, 0x62, 0x47, 0x55,
  0x74, 0x5a, 0x47, 0x55, 0x74, 0x52, 0x6e, 0x4a, 0x68, 0x62, 0x6d, 0x4e,
  0x6c, 0x4d, 0x52, 0x4d, 0x77, 0x45, 0x51, 0x59, 0x44, 0x56, 0x51, 0x51,
  0x4b, 0x44, 0x41, 0x70, 0x4a, 0x62, 0x6d, 0x5a, 0x70, 0x62, 0x6d, 0x6c,
  0x30, 0x0a, 0x4c, 0x6d, 0x6c, 0x76, 0x4d, 0x52, 0x51, 0x77, 0x45, 0x67,
  0x59, 0x44, 0x56, 0x51, 0x51, 0x4c, 0x44, 0x41, 0x74, 0x45, 0x5a, 0x58,
  0x5a, 0x6c, 0x62, 0x47, 0x39, 0x77, 0x62, 0x57, 0x56, 0x75, 0x64, 0x44,
  0x45, 0x58, 0x4d, 0x42, 0x55, 0x47, 0x41, 0x31, 0x55, 0x45, 0x41, 0x77,
  0x77, 0x4f, 0x54, 0x47, 0x39, 0x31, 0x61, 0x58, 0x4d, 0x67, 0x52, 0x6b,
  0x56, 0x56, 0x56, 0x6c, 0x4a, 0x4a, 0x0a, 0x52, 0x56, 0x49, 0x78, 0x4b,
  0x44, 0x41, 0x6d, 0x42, 0x67, 0x6b, 0x71, 0x68, 0x6b, 0x69, 0x47, 0x39,
  0x77, 0x30, 0x42, 0x43, 0x51, 0x45, 0x57, 0x47, 0x57, 0x78, 0x76, 0x64,
  0x57, 0x6c, 0x7a, 0x4c, 0x6d, 0x5a, 0x6c, 0x64, 0x58, 0x5a, 0x79, 0x61,
  0x57, 0x56, 0x79, 0x51, 0x47, 0x6c, 0x75, 0x5a, 0x6d, 0x6c, 0x75, 0x61,
  0x58, 0x51, 0x75, 0x61, 0x57, 0x38, 0x77, 0x67, 0x5a, 0x38, 0x77, 0x0a,
  0x44, 0x51, 0x59, 0x4a, 0x4b, 0x6f, 0x5a, 0x49, 0x68, 0x76, 0x63, 0x4e,
  0x41, 0x51, 0x45, 0x42, 0x42, 0x51, 0x41, 0x44, 0x67, 0x59, 0x30, 0x41,
  0x4d, 0x49, 0x47, 0x4a, 0x41, 0x6f, 0x47, 0x42, 0x41, 0x4e, 0x44, 0x2f,
  0x6f, 0x45, 0x54, 0x50, 0x35, 0x51, 0x37, 0x45, 0x76, 0x79, 0x4a, 0x54,
  0x32, 0x41, 0x35, 0x70, 0x57, 0x68, 0x6f, 0x2b, 0x48, 0x58, 0x6a, 0x4a,
  0x55, 0x6f, 0x57, 0x41, 0x0a, 0x48, 0x4f, 0x58, 0x74, 0x39, 0x53, 0x73,
  0x48, 0x2f, 0x4d, 0x63, 0x72, 0x79, 0x68, 0x68, 0x31, 0x39, 0x35, 0x46,
  0x61, 0x6b, 0x6e, 0x6c, 0x56, 0x39, 0x75, 0x6c, 0x43, 0x34, 0x4f, 0x43,
  0x6a, 0x33, 0x2f, 0x73, 0x2b, 0x74, 0x45, 0x4c, 0x51, 0x53, 0x2f, 0x58,
  0x72, 0x42, 0x55, 0x51, 0x79, 0x4c, 0x45, 0x71, 0x46, 0x30, 0x4f, 0x47,
  0x6a, 0x62, 0x4b, 0x72, 0x77, 0x41, 0x66, 0x63, 0x53, 0x0a, 0x78, 0x41,
  0x37, 0x2f, 0x41, 0x4b, 0x46, 0x30, 0x30, 0x6d, 0x55, 0x67, 0x32, 0x31,
  0x6f, 0x44, 0x65, 0x4d, 0x64, 0x55, 0x6f, 0x62, 0x7a, 0x58, 0x67, 0x2f,
  0x2b, 0x58, 0x65, 0x4c, 0x30, 0x55, 0x31, 0x6c, 0x45, 0x31, 0x75, 0x6d,
  0x41, 0x62, 0x2f, 0x35, 0x53, 0x2f, 0x66, 0x6b, 0x78, 0x45, 0x71, 0x38,
  0x79, 0x32, 0x6b, 0x77, 0x78, 0x71, 0x58, 0x34, 0x61, 0x4f, 0x4b, 0x47,
  0x75, 0x52, 0x0a, 0x37, 0x53, 0x69, 0x36, 0x53, 0x6d, 0x39, 0x50, 0x4d,
  0x7a, 0x39, 0x46, 0x41, 0x67, 0x4d, 0x42, 0x41, 0x41, 0x47, 0x6a, 0x65,
  0x7a, 0x42, 0x35, 0x4d, 0x41, 0x6b, 0x47, 0x41, 0x31, 0x55, 0x64, 0x45,
  0x77, 0x51, 0x43, 0x4d, 0x41, 0x41, 0x77, 0x4c, 0x41, 0x59, 0x4a, 0x59,
  0x49, 0x5a, 0x49, 0x41, 0x59, 0x62, 0x34, 0x51, 0x67, 0x45, 0x4e, 0x42,
  0x42, 0x38, 0x57, 0x48, 0x55, 0x39, 0x77, 0x0a, 0x5a, 0x57, 0x35, 0x54,
  0x55, 0x30, 0x77, 0x67, 0x52, 0x32, 0x56, 0x75, 0x5a, 0x58, 0x4a, 0x68,
  0x64, 0x47, 0x56, 0x6b, 0x49, 0x45, 0x4e, 0x6c, 0x63, 0x6e, 0x52, 0x70,
  0x5a, 0x6d, 0x6c, 0x6a, 0x59, 0x58, 0x52, 0x6c, 0x4d, 0x42, 0x30, 0x47,
  0x41, 0x31, 0x55, 0x64, 0x44, 0x67, 0x51, 0x57, 0x42, 0x42, 0x54, 0x56,
  0x58, 0x63, 0x59, 0x73, 0x71, 0x78, 0x2b, 0x39, 0x4b, 0x6c, 0x76, 0x6f,
  0x0a, 0x54, 0x38, 0x5a, 0x6f, 0x75, 0x4c, 0x73, 0x79, 0x68, 0x39, 0x51,
  0x64, 0x35, 0x54, 0x41, 0x66, 0x42, 0x67, 0x4e, 0x56, 0x48, 0x53, 0x4d,
  0x45, 0x47, 0x44, 0x41, 0x57, 0x67, 0x42, 0x51, 0x33, 0x69, 0x48, 0x58,
  0x44, 0x54, 0x6b, 0x73, 0x54, 0x38, 0x77, 0x4a, 0x2f, 0x44, 0x4c, 0x33,
  0x51, 0x6e, 0x49, 0x56, 0x47, 0x2b, 0x62, 0x39, 0x42, 0x33, 0x7a, 0x41,
  0x4e, 0x42, 0x67, 0x6b, 0x71, 0x0a, 0x68, 0x6b, 0x69, 0x47, 0x39, 0x77,
  0x30, 0x42, 0x41, 0x51, 0x55, 0x46, 0x41, 0x41, 0x4f, 0x42, 0x67, 0x51,
  0x41, 0x71, 0x5a, 0x63, 0x6b, 0x78, 0x2b, 0x65, 0x2b, 0x31, 0x64, 0x2b,
  0x64, 0x32, 0x2b, 0x67, 0x78, 0x4a, 0x32, 0x6a, 0x4e, 0x6a, 0x71, 0x46,
  0x69, 0x44, 0x67, 0x64, 0x72, 0x58, 0x63, 0x6d, 0x58, 0x35, 0x6e, 0x41,
  0x62, 0x68, 0x6c, 0x6d, 0x62, 0x38, 0x69, 0x4b, 0x6a, 0x45, 0x0a, 0x48,
  0x49, 0x2b, 0x2b, 0x5a, 0x70, 0x67, 0x39, 0x32, 0x36, 0x35, 0x69, 0x77,
  0x2f, 0x78, 0x79, 0x2f, 0x43, 0x76, 0x58, 0x62, 0x4a, 0x63, 0x4f, 0x48,
  0x46, 0x4c, 0x35, 0x7a, 0x50, 0x33, 0x6c, 0x38, 0x75, 0x6d, 0x32, 0x34,
  0x52, 0x67, 0x51, 0x4d, 0x6d, 0x77, 0x56, 0x6a, 0x32, 0x79, 0x6b, 0x4c,
  0x64, 0x58, 0x42, 0x57, 0x59, 0x47, 0x65, 0x64, 0x4c, 0x4c, 0x31, 0x31,
  0x55, 0x65, 0x43, 0x0a, 0x51, 0x69, 0x6f, 0x57, 0x55, 0x6f, 0x49, 0x4b,
  0x76, 0x43, 0x70, 0x7a, 0x43, 0x66, 0x36, 0x4d, 0x70, 0x6b, 0x41, 0x4d,
  0x53, 0x51, 0x62, 0x37, 0x46, 0x50, 0x74, 0x59, 0x77, 0x45, 0x39, 0x69,
  0x50, 0x4f, 0x44, 0x4b, 0x74, 0x73, 0x68, 0x6d, 0x6a, 0x58, 0x66, 0x41,
  0x61, 0x32, 0x72, 0x53, 0x63, 0x35, 0x65, 0x6c, 0x4d, 0x77, 0x79, 0x7a,
  0x30, 0x30, 0x73, 0x4a, 0x6b, 0x77, 0x3d, 0x3d, 0x0a, 0x2d, 0x2d, 0x2d,
  0x2d, 0x2d, 0x45, 0x4e, 0x44, 0x20, 0x43, 0x45, 0x52, 0x54, 0x49, 0x46,
  0x49, 0x43, 0x41, 0x54, 0x45, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a,
};

const std::vector<char> server_key =
{
  0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50,
  0x52, 0x49, 0x56, 0x41, 0x54, 0x45, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d,
  0x2d, 0x2d, 0x2d, 0x0a, 0x4d, 0x49, 0x49, 0x43, 0x65, 0x41, 0x49, 0x42,
  0x41, 0x44, 0x41, 0x4e, 0x42, 0x67, 0x6b, 0x71, 0x68, 0x6b, 0x69, 0x47,
  0x39, 0x77, 0x30, 0x42, 0x41, 0x51, 0x45, 0x46, 0x41, 0x41, 0x53, 0x43,
  0x41, 0x6d, 0x49, 0x77, 0x67, 0x67, 0x4a, 0x65, 0x41, 0x67, 0x45, 0x41,
  0x41, 0x6f, 0x47, 0x42, 0x41, 0x4e, 0x44, 0x2f, 0x6f, 0x45, 0x54, 0x50,
  0x35, 0x51, 0x37, 0x45, 0x76, 0x79, 0x4a, 0x54, 0x0a, 0x32, 0x41, 0x35,
  0x70, 0x57, 0x68, 0x6f, 0x2b, 0x48, 0x58, 0x6a, 0x4a, 0x55, 0x6f, 0x57,
  0x41, 0x48, 0x4f, 0x58, 0x74, 0x39, 0x53, 0x73, 0x48, 0x2f, 0x4d, 0x63,
  0x72, 0x79, 0x68, 0x68, 0x31, 0x39, 0x35, 0x46, 0x61, 0x6b, 0x6e, 0x6c,
  0x56, 0x39, 0x75, 0x6c, 0x43, 0x34, 0x4f, 0x43, 0x6a, 0x33, 0x2f, 0x73,
  0x2b, 0x74, 0x45, 0x4c, 0x51, 0x53, 0x2f, 0x58, 0x72, 0x42, 0x55, 0x51,
  0x79, 0x0a, 0x4c, 0x45, 0x71, 0x46, 0x30, 0x4f, 0x47, 0x6a, 0x62, 0x4b,
  0x72, 0x77, 0x41, 0x66, 0x63, 0x53, 0x78, 0x41, 0x37, 0x2f, 0x41, 0x4b,
  0x46, 0x30, 0x30, 0x6d, 0x55, 0x67, 0x32, 0x31, 0x6f, 0x44, 0x65, 0x4d,
  0x64, 0x55, 0x6f, 0x62, 0x7a, 0x58, 0x67, 0x2f, 0x2b, 0x58, 0x65, 0x4c,
  0x30, 0x55, 0x31, 0x6c, 0x45, 0x31, 0x75, 0x6d, 0x41, 0x62, 0x2f, 0x35,
  0x53, 0x2f, 0x66, 0x6b, 0x78, 0x45, 0x0a, 0x71, 0x38, 0x79, 0x32, 0x6b,
  0x77, 0x78, 0x71, 0x58, 0x34, 0x61, 0x4f, 0x4b, 0x47, 0x75, 0x52, 0x37,
  0x53, 0x69, 0x36, 0x53, 0x6d, 0x39, 0x50, 0x4d, 0x7a, 0x39, 0x46, 0x41,
  0x67, 0x4d, 0x42, 0x41, 0x41, 0x45, 0x43, 0x67, 0x59, 0x42, 0x53, 0x4c,
  0x4c, 0x41, 0x76, 0x58, 0x69, 0x36, 0x4a, 0x36, 0x41, 0x48, 0x65, 0x31,
  0x57, 0x69, 0x57, 0x41, 0x67, 0x5a, 0x54, 0x57, 0x79, 0x6a, 0x72, 0x0a,
  0x58, 0x50, 0x7a, 0x39, 0x55, 0x4b, 0x6f, 0x4d, 0x48, 0x63, 0x76, 0x50,
  0x35, 0x34, 0x77, 0x55, 0x49, 0x37, 0x75, 0x4b, 0x63, 0x70, 0x65, 0x73,
  0x70, 0x78, 0x67, 0x41, 0x62, 0x54, 0x52, 0x76, 0x38, 0x73, 0x50, 0x49,
  0x6a, 0x36, 0x5a, 0x35, 0x65, 0x75, 0x59, 0x56, 0x66, 0x79, 0x44, 0x65,
  0x79, 0x46, 0x47, 0x42, 0x78, 0x74, 0x68, 0x7a, 0x56, 0x4c, 0x6f, 0x54,
  0x78, 0x36, 0x65, 0x5a, 0x0a, 0x46, 0x4d, 0x70, 0x35, 0x47, 0x68, 0x42,
  0x37, 0x57, 0x64, 0x69, 0x33, 0x59, 0x55, 0x77, 0x50, 0x56, 0x66, 0x4c,
  0x53, 0x73, 0x4c, 0x5a, 0x41, 0x42, 0x71, 0x6c, 0x64, 0x4b, 0x36, 0x74,
  0x6e, 0x4d, 0x39, 0x54, 0x6f, 0x4a, 0x50, 0x38, 0x30, 0x39, 0x2f, 0x4d,
  0x6a, 0x66, 0x4e, 0x44, 0x32, 0x64, 0x36, 0x59, 0x2f, 0x52, 0x75, 0x30,
  0x55, 0x36, 0x73, 0x6f, 0x72, 0x71, 0x50, 0x55, 0x76, 0x0a, 0x56, 0x77,
  0x73, 0x49, 0x4b, 0x78, 0x42, 0x71, 0x57, 0x69, 0x62, 0x6e, 0x56, 0x50,
  0x74, 0x35, 0x41, 0x51, 0x4a, 0x42, 0x41, 0x50, 0x53, 0x68, 0x59, 0x34,
  0x72, 0x31, 0x63, 0x47, 0x38, 0x66, 0x56, 0x70, 0x6e, 0x6b, 0x6e, 0x4b,
  0x45, 0x64, 0x67, 0x31, 0x50, 0x47, 0x78, 0x77, 0x46, 0x68, 0x47, 0x37,
  0x56, 0x44, 0x42, 0x36, 0x74, 0x57, 0x51, 0x37, 0x62, 0x6d, 0x6a, 0x35,
  0x4e, 0x51, 0x0a, 0x53, 0x5a, 0x43, 0x64, 0x64, 0x65, 0x51, 0x5a, 0x6d,
  0x47, 0x66, 0x62, 0x4f, 0x6b, 0x48, 0x6c, 0x31, 0x58, 0x4e, 0x5a, 0x79,
  0x67, 0x56, 0x2b, 0x31, 0x31, 0x6c, 0x6c, 0x71, 0x7a, 0x58, 0x53, 0x38,
  0x51, 0x52, 0x37, 0x69, 0x57, 0x54, 0x68, 0x41, 0x4b, 0x30, 0x43, 0x51,
  0x51, 0x44, 0x61, 0x74, 0x6b, 0x6f, 0x4e, 0x71, 0x72, 0x56, 0x38, 0x68,
  0x6a, 0x6e, 0x72, 0x38, 0x61, 0x55, 0x4a, 0x0a, 0x46, 0x50, 0x63, 0x55,
  0x30, 0x2f, 0x4f, 0x79, 0x77, 0x33, 0x37, 0x54, 0x7a, 0x39, 0x6a, 0x4f,
  0x38, 0x64, 0x75, 0x74, 0x47, 0x34, 0x35, 0x7a, 0x59, 0x67, 0x47, 0x78,
  0x52, 0x61, 0x63, 0x33, 0x37, 0x77, 0x4e, 0x39, 0x51, 0x30, 0x38, 0x38,
  0x76, 0x38, 0x37, 0x50, 0x4d, 0x62, 0x6c, 0x72, 0x38, 0x56, 0x38, 0x31,
  0x63, 0x39, 0x5a, 0x51, 0x6b, 0x6e, 0x35, 0x2f, 0x2f, 0x53, 0x50, 0x4b,
  0x0a, 0x56, 0x39, 0x50, 0x35, 0x41, 0x6b, 0x45, 0x41, 0x67, 0x65, 0x39,
  0x2f, 0x4b, 0x66, 0x33, 0x33, 0x2f, 0x47, 0x34, 0x4f, 0x31, 0x36, 0x73,
  0x41, 0x4c, 0x75, 0x75, 0x34, 0x4a, 0x37, 0x56, 0x37, 0x57, 0x70, 0x59,
  0x7a, 0x32, 0x33, 0x47, 0x42, 0x44, 0x31, 0x62, 0x41, 0x6e, 0x4e, 0x4f,
  0x57, 0x43, 0x30, 0x38, 0x6e, 0x34, 0x2f, 0x4a, 0x65, 0x2f, 0x67, 0x74,
  0x43, 0x55, 0x6c, 0x65, 0x31, 0x0a, 0x64, 0x2b, 0x38, 0x57, 0x45, 0x7a,
  0x44, 0x73, 0x42, 0x30, 0x4d, 0x36, 0x4b, 0x7a, 0x65, 0x2f, 0x57, 0x74,
  0x56, 0x79, 0x51, 0x30, 0x6c, 0x43, 0x7a, 0x78, 0x78, 0x62, 0x2b, 0x51,
  0x4a, 0x42, 0x41, 0x4a, 0x79, 0x49, 0x5a, 0x50, 0x33, 0x64, 0x46, 0x4f,
  0x46, 0x58, 0x79, 0x2f, 0x4c, 0x44, 0x55, 0x77, 0x50, 0x70, 0x2f, 0x6d,
  0x44, 0x6f, 0x78, 0x58, 0x31, 0x48, 0x44, 0x2b, 0x6d, 0x57, 0x0a, 0x30,
  0x36, 0x78, 0x68, 0x53, 0x34, 0x46, 0x63, 0x76, 0x4a, 0x70, 0x32, 0x4a,
  0x5a, 0x48, 0x7a, 0x73, 0x52, 0x65, 0x47, 0x4f, 0x44, 0x41, 0x5a, 0x30,
  0x59, 0x64, 0x41, 0x48, 0x45, 0x73, 0x4d, 0x59, 0x70, 0x49, 0x51, 0x41,
  0x62, 0x31, 0x6d, 0x39, 0x35, 0x64, 0x5a, 0x45, 0x62, 0x4b, 0x57, 0x77,
  0x56, 0x76, 0x62, 0x65, 0x6a, 0x6b, 0x43, 0x51, 0x51, 0x44, 0x52, 0x46,
  0x42, 0x39, 0x73, 0x0a, 0x66, 0x39, 0x68, 0x53, 0x33, 0x57, 0x4d, 0x69,
  0x38, 0x65, 0x47, 0x6e, 0x43, 0x4d, 0x33, 0x58, 0x34, 0x4c, 0x32, 0x32,
  0x7a, 0x68, 0x61, 0x46, 0x42, 0x74, 0x68, 0x33, 0x73, 0x51, 0x7a, 0x78,
  0x72, 0x63, 0x4a, 0x35, 0x51, 0x4a, 0x55, 0x4d, 0x78, 0x57, 0x69, 0x44,
  0x6c, 0x45, 0x4f, 0x72, 0x76, 0x71, 0x47, 0x77, 0x4c, 0x48, 0x79, 0x67,
  0x50, 0x4f, 0x38, 0x59, 0x59, 0x38, 0x58, 0x6c, 0x0a, 0x74, 0x79, 0x59,
  0x63, 0x45, 0x64, 0x6c, 0x43, 0x4c, 0x35, 0x36, 0x71, 0x74, 0x41, 0x34,
  0x59, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e, 0x44, 0x20, 0x50,
  0x52, 0x49, 0x56, 0x41, 0x54, 0x45, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d,
  0x2d, 0x2d, 0x2d, 0x0a,
};

const std::vector<char> server_dh1024 =
{
  0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x44,
  0x48, 0x20, 0x50, 0x41, 0x52, 0x41, 0x4d, 0x45, 0x54, 0x45, 0x52, 0x53,
  0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a, 0x4d, 0x49, 0x47, 0x48, 0x41, 0x6f,
  0x47, 0x42, 0x41, 0x4c, 0x50, 0x35, 0x33, 0x68, 0x32, 0x31, 0x59, 0x6d,
  0x6a, 0x47, 0x4a, 0x4f, 0x34, 0x53, 0x2f, 0x38, 0x42, 0x7a, 0x44, 0x61,
  0x57, 0x63, 0x4b, 0x6f, 0x46, 0x6c, 0x6a, 0x56, 0x32, 0x46, 0x37, 0x4f,
  0x66, 0x39, 0x4e, 0x50, 0x33, 0x31, 0x33, 0x4b, 0x54, 0x42, 0x2f, 0x51,
  0x55, 0x62, 0x6c, 0x58, 0x70, 0x6e, 0x65, 0x4e, 0x2b, 0x62, 0x0a, 0x32,
  0x41, 0x70, 0x76, 0x4e, 0x31, 0x32, 0x69, 0x33, 0x51, 0x46, 0x75, 0x52,
  0x2b, 0x4a, 0x6e, 0x4e, 0x70, 0x2b, 0x51, 0x2f, 0x6d, 0x47, 0x7a, 0x34,
  0x6b, 0x77, 0x32, 0x2b, 0x45, 0x48, 0x6d, 0x35, 0x67, 0x65, 0x68, 0x38,
  0x6f, 0x35, 0x54, 0x47, 0x6a, 0x57, 0x6a, 0x57, 0x50, 0x69, 0x33, 0x42,
  0x30, 0x78, 0x6c, 0x4c, 0x76, 0x4e, 0x4a, 0x4c, 0x54, 0x31, 0x32, 0x68,
  0x41, 0x56, 0x42, 0x0a, 0x76, 0x31, 0x6f, 0x38, 0x65, 0x50, 0x68, 0x30,
  0x72, 0x5a, 0x65, 0x66, 0x43, 0x47, 0x7a, 0x74, 0x68, 0x32, 0x4a, 0x2f,
  0x6a, 0x4b, 0x4c, 0x4f, 0x41, 0x57, 0x46, 0x36, 0x30, 0x6e, 0x32, 0x53,
  0x33, 0x45, 0x36, 0x34, 0x37, 0x5a, 0x4a, 0x74, 0x74, 0x32, 0x2f, 0x4b,
  0x47, 0x47, 0x6b, 0x76, 0x37, 0x56, 0x33, 0x62, 0x41, 0x67, 0x45, 0x43,
  0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e, 0x44, 0x20, 0x44, 0x48,
  0x20, 0x50, 0x41, 0x52, 0x41, 0x4d, 0x45, 0x54, 0x45, 0x52, 0x53, 0x2d,
  0x2d, 0x2d, 0x2d, 0x2d, 0x0a,
};
