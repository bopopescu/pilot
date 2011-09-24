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
#include <core/kinematics/MoveMap.hh>
#include <protocols/moves/MinMover.hh>
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

typedef numeric::xyzVector<Real> Vec;
typedef numeric::xyzMatrix<Real> Mat;

static basic::Tracer TR("dunscores");



void run() {
	using namespace basic::options::OptionKeys;
	using namespace core::id;

	vector1<string> infiles;
	if( option[in::file::l].user() ) {
		utility::io::izstream in(option[in::file::l]()[1]);
		string tmp;
		while(in >> tmp) infiles.push_back(tmp);
	} else if(option[in::file::s].user()) {
		infiles = option[in::file::s]();
	} else {
		utility_exit_with_message("no input!");
	}

	core::scoring::ScoreFunctionOP sf = core::scoring::getScoreFunction(); //new core::scoring::ScoreFunction;
		//sf->set_weight(core::scoring::fa_dun,1.0);

	for(Size ifile = 1; ifile <= infiles.size(); ifile++) {
		string infile = infiles[ifile];
		Pose pose;
		pose_from_pdb(pose,infile);

		core::kinematics::MoveMapOP movemap = new core::kinematics::MoveMap;
		movemap->set_chi(true);
		movemap->set_bb(false);
		movemap->set_jump(false);
		protocols::moves::MinMover m( movemap, sf, "dfpmin_armijo_nonmonotone", 1e-5, true, false, false );
		m.apply(pose);

		sf->score(pose);
		for(Size i = 1; i <= pose.n_residue(); ++i) {
			vector1<Real> c(4,0.0);
			for(Size j = 1; j <= pose.residue(i).nchi(); ++j) c[j] = pose.chi(j,i);
			TR << utility::file_basename(infile) << " " << pose.residue(i).name3() << " " << pose.energies().residue_total_energies(i)[core::scoring::fa_dun] << " " << c[1] << " " << c[2] << " " << c[3] << " " << c[4] << std::endl;
		}
	}
}


int main (int argc, char *argv[]) {
	core::init(argc,argv);
	run();
}


