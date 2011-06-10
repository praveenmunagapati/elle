//
// ---------- header ----------------------------------------------------------
//
// project       elle
//
// license       infinit
//
// file          /home/mycure/infinit/elle/network/Door.hxx
//
// created       julien quintard   [tue feb 23 13:44:55 2010]
// updated       julien quintard   [fri jun  3 19:47:59 2011]
//

#ifndef ELLE_NETWORK_DOOR_HXX
#define ELLE_NETWORK_DOOR_HXX

//
// ---------- includes --------------------------------------------------------
//

#include <elle/standalone/Maid.hh>
#include <elle/standalone/Report.hh>

#include <elle/network/Packet.hh>

#include <elle/Manifest.hh>

namespace elle
{
  namespace network
  {

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method sends a packet.
    ///
    template <typename I>
    Status		Door::Send(const I			inputs,
				   const Event&			event)
    {
      Packet		packet;
      Header		header;
      Data		data;
      Natural64		offset;
      Natural64		point;

      enter();

      // create an data for the inputs.
      if (data.Create() == StatusError)
	escape("unable to create the data");

      // serialize the inputs.
      if (inputs.Serialize(data) == StatusError)
	escape("unable to serialize the inputs");

      // prepare the packet.
      if (packet.Create() == StatusError)
	escape("unable to create the packet");

      // retrieve the offset---i.e size attribute---of the packet at this
      // time so that it can be used later to update parts of the archive,
      // especially the header.
      offset = packet.size;

      // serialize the the header though, at this point, it has not
      // been created.
      if (packet.Serialize(header) == StatusError)
	escape("unable to serialize the header");

      // save the offset just following the header's serialization.
      point = packet.size;

      // serialize the the data.
      if (packet.Serialize(data) == StatusError)
	escape("unable to serialize the data");

      // create the header now that we know that final archive's size.
      if (header.Create(event, inputs.tag, packet.size - point) == StatusError)
	escape("unable to create the header");

      // update the header.
      if (packet.Update(offset, header) == StatusError)
	escape("unable to update the header");

      // write the socket.
      if (this->Write(packet) == StatusError)
	escape("unable to write the packet");

      leave();
    }

    ///
    /// XXX
    ///
    /// note that the session is never assigned when receive is used
    /// to manually receive messages. it is therefore the caller's
    /// responsability to keep information regarding who it is communicating
    /// with and using which socket.
    ///
    template <typename O>
    Status		Door::Receive(const Event&		event,
				      O				outputs)
    {
      Report		report;
      Parcel*		parcel;

      enter(instance(parcel));

      // process the call depending on the mode.
      if (this->mode == Socket::ModeAsynchronous)
	{
	  // block the current fiber until the given event is received.
	  if (Fiber::Wait(event, parcel) == StatusError)
	    escape("an error occured while waiting for a specific event");
	}
      else
	{
	  // synchronously read data from the socket i.e block if not
	  // available meaning that no even can be processed until
	  // the data is received.

	  // until a complete parcel has been received.
	  while (true)
	    {
	      Status	status;

	      // wait for the ready signal.
	      if (this->socket->waitForReadyRead() == false)
		escape("an error occured while waiting for the ready signal");

	      // try to read the parcel.
	      switch (this->Read(parcel))
		{
		case StatusFalse:
		  {
		    // since the parcel is not complete, keep waiting...
		    continue;
		  }
		case StatusError:
		  {
		    escape("unable to read a complete parcel from the door");
		  }
		};

	      // at this point, we should have a complete parcel.
	      break;
	    }
	}

      // check the tag.
      if (parcel->header->tag != outputs.tag)
	{
	  // test if the message received is an error, if so, append
	  // the report to the local one.
	  if (parcel->header->tag == TagError)
	    {
	      // extract the error message.
	      if (report.Extract(*parcel->data) == StatusError)
		escape("unable to extract the error message");

	      // report the remote error.
	      report(Report::TypeError, report);
	    }

	  escape("received a packet with an unexpected tag");
	}

      // extract the arguments.
      if (outputs.Extract(*parcel->data) == StatusError)
	escape("unable to extract the arguments");

      // delete the parcel.
      delete parcel;

      // stop tracking the parcel since it has just been deleted.
      waive(parcel);

      leave();
    }

    ///
    /// XXX
    ///
    template <typename I,
	      typename O>
    Status		Door::Call(const I			inputs,
				   O				outputs)
    {
      Event		event;

      enter();

      // generate an event to link the request with the response.
      if (event.Generate() == StatusError)
	escape("unable to generate the event");

      // send the inputs.
      if (this->Send(inputs, event) == StatusError)
	escape("unable to send the inputs");

      // wait for the reply.
      if (this->Receive(event, outputs) == StatusError)
	escape("unable to receive the outputs");

      leave();
    }

    ///
    /// XXX
    ///
    template <typename I>
    Status		Door::Reply(const I			inputs)
    {
      Session*		session;

      enter();

      // retrieve the current session.
      if (Session::Instance(session) == StatusError)
	escape("unable to retrieve the session instance");

      // send a message as a response by using the event of
      // the received message i.e the current session.
      if (this->Send(inputs, session->event) == StatusError)
	escape("unable to send the reply");

      leave();
    }

  }
}

#endif
