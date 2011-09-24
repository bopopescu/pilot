// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// This file is part of the Rosetta software suite and is made available under license.
// The Rosetta software is developed by the contributing members of the Rosetta Commons consortium.
// (C) 199x-2009 Rosetta Commons participating institutions and developers.
// For more information, see http://www.rosettacommons.org/.

/// @file /src/apps/pilat/will/sup_test.cc
/// @brief test rms_util superimpose stuff

#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/option.hh>
#include <core/id/AtomID_Map.hh>
#include <core/import_pose/import_pose.hh>
#include <core/init.hh>
#include <core/io/pdb/pose_io.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/scoring/rms_util.hh>


int main (int argc, char *argv[])
{
	
	core::init(argc,argv);
	
	core::pose::Pose mod_pose,ref_pose;
	
	core::import_pose::pose_from_pdb(mod_pose,basic::options::option[basic::options::OptionKeys::in::file::s]()[1]);
	core::import_pose::pose_from_pdb(ref_pose,basic::options::option[basic::options::OptionKeys::in::file::s]()[2]);	
	
	using namespace core::id;
	AtomID_Map<AtomID> atom_map;
	core::pose::initialize_atomid_map(atom_map,mod_pose,BOGUS_ATOM_ID);
	for(core::Size ir = 1; ir <= 6; ++ir) {
		for(core::Size ia = 1; ia <= 4; ia++) {
			core::Size ref_rsd = ref_pose.n_residue() - 6 + ir;
			atom_map[ AtomID(ia,ir) ] = AtomID(ia,ref_rsd);
		}
	}
	
	core::Real rms = core::scoring::superimpose_pose(mod_pose,ref_pose,atom_map);
	
	std::cout << "aligned region rms: " << rms << std::endl;
	
	mod_pose.dump_pdb("moved_pose.pdb");
	ref_pose.dump_pdb("fixed_pose.pdb");

	return 0;
}







