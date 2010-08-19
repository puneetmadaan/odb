// file      : odb/validator.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <iostream>

#include <odb/traversal.hxx>
#include <odb/validator.hxx>

using namespace std;

namespace
{
  struct data_member: traversal::data_member
  {
    data_member (bool& valid)
        : valid_ (valid)
    {
    }

    virtual void
    traverse (type& m)
    {
      if (m.count ("transient"))
        return;

      count_++;
      semantics::type& type (m.type ());
      semantics::belongs& b (m.belongs ());

      if (type.fq_anonymous (m.belongs ().hint ()))
      {
        // Can be a template-id (which we should handle eventually) or an
        // anonymous type in member declaration (e.g., struct {...} m_;).
        //
        cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " error: unnamed type in data member declaration" << endl;

        cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " info: use 'typedef' to name this type" << endl;

        valid_ = false;
      }

      if (m.count ("id"))
      {
        if (id_ != 0)
        {
          cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
               << " error: multiple object id members" << endl;

          cerr << id_->file () << ":" << id_->line () << ":" << id_->column ()
               << ": info: previous id member declared here" << endl;

          valid_ = false;
        }

        id_ = &m;
      }
    }

    bool& valid_;
    size_t count_;
    semantics::data_member* id_;
  };

  struct class_: traversal::class_
  {
    class_ (bool& valid, semantics::unit& unit)
        : valid_ (valid), unit_ (unit), member_ (valid)
    {
      *this >> names_ >> member_;
    }


    virtual void
    traverse (type& c)
    {
      if (c.file () != unit_.file () || !c.count ("object"))
        return;

      member_.count_ = 0;
      member_.id_ = 0;

      names (c);

      if (member_.id_ == 0)
      {
        cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: no data member designated as object id" << endl;

        cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " info: use '#pragma odb id' to specify object id member"
             << endl;

        valid_ = false;
      }

      if (member_.count_ == 0)
      {
        cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: no persistent data members in the class" << endl;

        valid_ = false;
      }
    }

    bool& valid_;
    semantics::unit& unit_;

    data_member member_;
    traversal::names names_;
  };
}

bool validator::
validate (options const&,
          semantics::unit& u,
          semantics::path const&)
{
  bool valid (true);

  traversal::unit unit;
  traversal::defines unit_defines;
  traversal::namespace_ ns;
  class_ c (valid, u);

  unit >> unit_defines >> ns;
  unit_defines >> c;

  traversal::defines ns_defines;

  ns >> ns_defines >> ns;
  ns_defines >> c;

  unit.dispatch (u);

  return valid;
}

validator::
validator ()
{
}
