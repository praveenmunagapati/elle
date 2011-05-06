//
// ---------- header ----------------------------------------------------------
//
// project       etoile
//
// license       infinit
//
// file          /home/mycure/infinit/etoile/depot/Record.cc
//
// created       julien quintard   [thu dec  3 03:11:13 2009]
// updated       julien quintard   [thu may  5 16:50:50 2011]
//

//
// ---------- includes --------------------------------------------------------
//

#include <etoile/depot/Record.hh>
#include <etoile/depot/Repository.hh>

namespace etoile
{
  namespace depot
  {

//
// ---------- constructors & destructors --------------------------------------
//

    ///
    /// default constructor
    ///
    Record::Record():
      location(LocationUnknown),
      timer(NULL)
    {
    }

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method initializes a record with the address of the block
    /// it is associated.
    ///
    /// this method allocates a timer for blocks which can expires, i.e
    /// mutable blocks, such as PKBs.
    ///
    elle::Status	Record::Create(const nucleus::Address&	address)
    {
      enter();

      // set the record address.
      this->address = address;
      /* XXX
      // check if this family of block expires.
      if (Repository::Delays[address.family] != NULL)
	{
	  elle::Callback<>	discard(&Record::Discard, this);

	  // allocate a new timer object.
	  this->timer = new elle::Timer;

	  // set up the timer.
	  if (this->timer->Create(elle::Timer::ModeSingle,
				  discard) == elle::StatusError)
	    escape("unable to set up the timer");

	  // note that the timer is not started yet. it will be launched
	  // once the Timer() method has been called.
	}
      */
      leave();
    }

    ///
    /// this method must be called whenever the timer is to be re-computed
    /// and restarted.
    ///
    elle::Status	Record::Monitor()
    {
      elle::Time*	time;
      elle::Natural64	expiration;

      enter();

      // if no timer is required for the family of block, just return.
      if (this->timer == NULL)
	leave();

      // stop a potentially already existing timer.
      if (this->timer->Stop() == elle::StatusError)
	escape("unable to stop the timer");
      /* XXX
      // retrieve the time.
      time = Repository::Delays[this->address.family];
      */
      // compute the number of seconds. note that day, minute and year
      // are grossly computed.
      expiration = time->second + time->minute * 60 + time->hour * 3600 +
	time->day * 86400 + time->month * 2592000 + time->year * 31104000;

      // start the timer, in milli-seconds.
      if (this->timer->Start(expiration * 1000) == elle::StatusError)
	escape("unable to restart the timer");

      leave();
    }

    ///
    /// this method releases every resource allocated by the record.
    ///
    elle::Status	Record::Destroy()
    {
      enter();

      ///
      /// first, stop and release the existing timer.
      ///
      {
	// release the timer, if there is one.
	if (this->timer != NULL)
	  {
	    // stop the timer.
	    if (this->timer->Stop() == elle::StatusError)
	      escape("unable to stop the timer");

	    // delete it.
	    delete this->timer;
	  }
      }

      ///
      /// then, destroy the data.
      ///
      {
	switch (this->location)
	  {
	  case LocationCache:
	    {
	      // destroy the cell.
	      if (this->cell->Destroy() == elle::StatusError)
		escape("unable to destroy the cell");

	      // release the cell.
	      delete this->cell;

	      break;
	    }
	  case LocationReserve:
	    {
	      // destroy the unit.
	      if (this->unit->Destroy() == elle::StatusError)
		escape("unable to destroy the unit");

	      // release the unit.
	      delete this->unit;

	      break;
	    }
	  case LocationUnknown:
	    {
	      escape("unable to locate the data");
	    }
	  }
      }

      leave();
    }

//
// ---------- dumpable --------------------------------------------------------
//

    ///
    /// this method dumps the record attributes.
    ///
    elle::Status	Record::Dump(const elle::Natural32	margin) const
    {
      elle::String	alignment(margin, ' ');

      enter();

      std::cout << alignment << "[Record] "
		<< this << std::endl;

      // dump the address.
      if (this->address.Dump(margin + 2) == elle::StatusError)
	escape("unable to dump the address");

      // dump the content.
      switch (this->location)
	{
	case LocationCache:
	  {
	    if (this->cell->Dump(margin + 2) == elle::StatusError)
	      escape("unable to dump the cell");

	    break;
	  }
	case LocationReserve:
	  {
	    if (this->unit->Dump(margin + 2) == elle::StatusError)
	      escape("unable to dump the unit");

	    break;
	  }
	case LocationUnknown:
	  {
	    escape("unknown location");
	  }
	}
    }

//
// ---------- callbacks -------------------------------------------------------
//

    ///
    /// this method is called whenever the element timeouts i.e must be
    /// removed from the repository.
    ///
    elle::Status	Record::Discard()
    {
      enter();

      // remove the block from the repository.
      if (Repository::Discard(this->address) == elle::StatusError)
	escape("unable to discard the timeout block");

      leave();
    }

  }
}
