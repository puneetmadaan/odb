// file      : odb/semantics/relational/primary-key.hxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_PRIMARY_KEY_HXX
#define ODB_SEMANTICS_RELATIONAL_PRIMARY_KEY_HXX

#include <odb/semantics/relational/elements.hxx>
#include <odb/semantics/relational/key.hxx>

namespace semantics
{
  namespace relational
  {
    class primary_key: public unameable, public key
    {
    public:
      bool
      auto_ () const
      {
        return auto__;
      }

    public:
      primary_key (bool auto_)
          // Primary key has the implicit empty id.
          //
          : unameable (""), auto__ (auto_)
      {
      }

      virtual string
      kind () const
      {
        return "primary key";
      }

    private:
      bool auto__;
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_PRIMARY_KEY_HXX
