//
// ---------- header ----------------------------------------------------------
//
// project       etoile
//
// license       infinit
//
// file          /home/mycure/infinit/etoile/components/Link.cc
//
// created       julien quintard   [fri aug 14 19:00:57 2009]
// updated       julien quintard   [thu may  5 16:42:20 2011]
//

//
// ---------- includes --------------------------------------------------------
//

#include <etoile/components/Link.hh>
#include <etoile/components/Rights.hh>
#include <etoile/components/Contents.hh>

#include <etoile/journal/Journal.hh>

#include <etoile/user/User.hh>

namespace etoile
{
  namespace components
  {

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method creates a link object.
    ///
    elle::Status	Link::Create(context::Link*		context)
    {
      user::User*	user;

      enter();

      // load the current user.
      if (user::User::Instance(user) == elle::StatusError)
	escape("unable to load the user");

      // allocate a new link object.
      context->object = new nucleus::Object;

      // create the irectory.
      if (context->object->Create(nucleus::GenreLink,
				  user->client->agent->K) == elle::StatusError)
	escape("unable to create the link object");

      // bind the object.
      if (context->object->Bind(context->address) == elle::StatusError)
	escape("unable to bind the object");

      leave();
    }

    ///
    /// this method loads the link object identified by the given
    /// address in the context.
    ///
    elle::Status	Link::Load(context::Link*		context,
				   const nucleus::Address&	address)
					
    {
      enter();

      // load the object.
      if (Object::Load(context, address) == elle::StatusError)
	escape("unable to load the object");

      // check that the object is a link.
      if (context->object->meta.genre != nucleus::GenreLink)
	escape("this object is not a link");

      leave();
    }

    ///
    /// this method binds a new target to the link.
    ///
    elle::Status	Link::Bind(context::Link*		context,
				   const path::Way&		way)
    {
      enter();

      // determine the rights.
      if (Rights::Determine(context) == elle::StatusError)
	escape("unable to determine the rights");

      // check if the current user has the right the write the reference.
      if (!(context->rights->record.permissions & nucleus::PermissionWrite))
	escape("unable to perform the operation without the permission");

      // open the contents.
      if (Contents::Open(context) == elle::StatusError)
	escape("unable to open the contents");

      // bind the link.
      if (context->contents->content->Bind(way.path) == elle::StatusError)
	escape("unable to bind the link");

      leave();
    }

    ///
    /// this method returns the way associated with this link.
    ///
    elle::Status	Link::Resolve(context::Link*		context,
				      path::Way&		way)
    {
      enter();

      // determine the rights.
      if (Rights::Determine(context) == elle::StatusError)
	escape("unable to determine the rights");

      // check if the current user has the right the read the reference.
      if (!(context->rights->record.permissions & nucleus::PermissionRead))
	escape("unable to perform the operation without the permission");

      // open the contents.
      if (Contents::Open(context) == elle::StatusError)
	escape("unable to open the contents");

      // resolve the link.
      if (context->contents->content->Resolve(way.path) == elle::StatusError)
	escape("unable to resolve the link");

      leave();
    }

    ///
    /// this method discards the modifications applied onto the context.
    ///
    elle::Status	Link::Discard(context::Link*		context)
    {
      enter();

      // discard the object's modifications.
      if (Object::Discard(context) == elle::StatusError)
	escape("unable to discard the object modifications");

      leave();
    }

    ///
    /// this store the modifications applied onto the link context.
    ///
    elle::Status	Link::Store(context::Link*		context)
    {
      user::User*	user;

      enter();

      // close the catalog.
      if (Contents::Close(context) == elle::StatusError)
	escape("unable to close the contents");

      // store the object.
      if (Object::Store(context) == elle::StatusError)
	escape("unable to store the object");

      leave();
    }

    ///
    /// this method removes the object along with the blocks attached to it.
    ///
    elle::Status	Link::Destroy(context::Link*		context)
    {
      user::User*	user;
      nucleus::Size	size;

      enter();

      // determine the rights.
      if (Rights::Determine(context) == elle::StatusError)
	escape("unable to determine the rights");

      // check if the current user is the object owner.
      if (context->rights->role != nucleus::RoleOwner)
	escape("unable to destroy a not-owned object");

      // destroy the contents.
      if (Contents::Destroy(context) == elle::StatusError)
	escape("unable to destroy the contents");

      // destroy the object.
      if (Object::Destroy(context) == elle::StatusError)
	escape("unable to destroy the object");

      leave();
    }

  }
}
