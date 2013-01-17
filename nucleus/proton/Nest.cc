#include <nucleus/proton/Nest.hh>

namespace nucleus
{
  namespace proton
  {
    /*-------------.
    | Construction |
    `-------------*/

    Nest::Nest(Limits const& limits,
               Network const& network,
               cryptography::PublicKey const& agent_K):
      _limits(limits),
      _network(network),
      _agent_K(agent_K)
    {
    }
  }
}
