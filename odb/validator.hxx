// file      : odb/validator.hxx
// copyright : Copyright (c) 2009-2015 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_VALIDATOR_HXX
#define ODB_VALIDATOR_HXX

#include <odb/options.hxx>
#include <odb/features.hxx>
#include <odb/semantics/unit.hxx>

class validator_failed {};

// The first pass is performed before processing. The second -- after.
//
void
validate (options const&,
          features&,
          semantics::unit&,
          semantics::path const&,
          unsigned short pass);

#endif // ODB_VALIDATOR_HXX
