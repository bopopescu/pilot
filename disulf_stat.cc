// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// This file is part of the Rosetta software suite and is made available under license.
// The Rosetta software is developed by the contributing members of the Rosetta Commons consortium.
// (C) 199x-2009 Rosetta Commons participating institutions and developers.
// For more information, see http://www.rosettacommons.org/.

/// @file /src/apps/pilat/will/genmatch.cc
/// @brief ???

#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <basic/options/keys/willmatch.OptionKeys.gen.hh>
#include <basic/options/option.hh>
#include <basic/options/util.hh>
#include <basic/Tracer.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/util.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/import_pose/import_pose.hh>
#include <core/init.hh>
#include <core/io/pdb/pose_io.hh>
#include <core/kinematics/Stub.hh>
#include <core/pack/optimizeH.hh>
#include <core/pose/annotated_sequence.hh>
#include <core/pose/Pose.hh>
#include <core/pose/symmetry/util.hh>
#include <core/pose/util.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/sasa.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/ScoringManager.hh>
#include <core/io/silent/ScoreFileSilentStruct.hh>
#include <core/io/silent/SilentFileData.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/xyz.io.hh>
#include <ObjexxFCL/FArray2D.hh>
#include <ObjexxFCL/format.hh>
#include <ObjexxFCL/string.functions.hh>
#include <protocols/scoring/ImplicitFastClashCheck.hh>
#include <sstream>
#include <utility/io/izstream.hh>
#include <utility/io/ozstream.hh>
// #include <devel/init.hh>

// #include <core/scoring/constraints/LocalCoordinateConstraint.hh>
#include "apps/pilot/will/will_util.hh"

using core::Real;
using core::Size;
using core::pose::Pose;
using core::kinematics::Stub;
using protocols::scoring::ImplicitFastClashCheck;
using std::string;
using utility::vector1;
using ObjexxFCL::string_of;
using ObjexxFCL::lead_zero_string_of;
using numeric::min;
using core::import_pose::pose_from_pdb;
using basic::options::option;

typedef utility::vector1<core::Real> Sizes;
typedef numeric::xyzVector<Real> Vec;
typedef numeric::xyzMatrix<Real> Mat;

static basic::Tracer TR("disulf_stat");

int main (int argc, char *argv[]) {
  using namespace basic::options::OptionKeys;

  core::init(argc,argv);

  vector1<Pose> poses;
  TR << "reading big data!" << std::endl;
  pose_from_pdb(poses,option[in::file::s]()[1],false);

  for(Size ip = 1; ip <= poses.size(); ++ip) {
    Pose const & pose(poses[ip]);
    for(Size ir = 1; ir <= pose.n_residue(); ++ir) {
			std::cout << "GLUCHI " << pose.chi(1,ir) << " " << pose.chi(2,ir) << " " << pose.chi(3,ir) << std::endl;
      // for(Size jr = ir+1; jr <= pose.n_residue(); ++jr) {
			// 	Real dgg2 = pose.residue(ir).xyz("SG").distance_squared(pose.residue(jr).xyz("SG"));
			// 	if( dgg2 > 9.0) continue;
			// 	Vec ca1 = pose.residue(ir).xyz("CA");
			// 	Vec cb1 = pose.residue(ir).xyz("CB");
			// 	Vec sg1 = pose.residue(ir).xyz("SG");
			// 	Vec ca2 = pose.residue(jr).xyz("CA");
			// 	Vec cb2 = pose.residue(jr).xyz("CB");
			// 	Vec sg2 = pose.residue(jr).xyz("SG");
			// 	Real dgg = sg1.distance(sg2);
			// 	Real ag1 = numeric::angle_degrees(       cb1,sg1,sg2        );
			// 	Real ag2 = numeric::angle_degrees(           sg1,sg2,cb2    );
			// 	Real dh1 = numeric::dihedral_degrees(    cb1,sg1,sg2,cb2    );
			// 	Real dh2 = numeric::dihedral_degrees(        sg1,sg2,cb2,ca2);
			// 	Real dh3 = numeric::dihedral_degrees(ca1,cb1,sg1,sg2        );
			// 	TR << "DISULF_STAT " << dgg << " " << ag1 << " " << ag2 << " " << dh1 << " " << dh2 << " " << dh3 << std::endl;
      // }
    }
  }

  TR << "DONE!" << std::endl;
}
