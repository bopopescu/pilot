// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file
/// @brief

#include <core/types.hh>
#include <devel/init.hh>

#include <core/pose/Pose.hh>
#include <core/io/pdb/pose_io.hh>
#include <core/scoring/packing/PoseBalls.hh>

#include <core/scoring/sasa.hh>

#include <core/scoring/packing/surf_vol.hh>

#include <basic/options/option.hh>

#include <utility/io/izstream.hh>
#include <utility/io/ozstream.hh>

#include <pstream.h>

#include <time.h>

// option key includes

#include <basic/options/keys/in.OptionKeys.gen.hh>

//Auto Headers
#include <core/import_pose/import_pose.hh>




std::map<std::string,utility::io::ozstream*> outs;

void test( std::string fname ) {
	using namespace std;
	using namespace core;
	using namespace io::pdb;
	using namespace pose;
	using namespace scoring;
	using namespace packing;
	using namespace basic::options;

	Pose pose;
	core::import_pose::pose_from_pdb(pose,fname);

	time_t t1 = clock();
	SurfVol sv = packing::get_surf_vol(pose,1.4);
	time_t t2 = clock();


	Real tot_sasa = calc_total_sasa( pose, 1.4 );
	time_t t3 = clock();

	// for( Size i = 1; i <= sv.surf.size(); i++ ) {
	// 	std::cerr << i << " " << sv.surf[id::AtomID(2,i)] << " " << sv.vol[id::AtomID(2,i)] << std::endl;
	// }
	Real cps = Real(CLOCKS_PER_SEC);
	std::cerr << "tot SASA & SAV  " << sv.tot_surf <<" "<< sv.tot_vol <<" time: "<< Real(t2-t1)/cps << std::endl;
	std::cerr << "tot SASA (dots) " << tot_sasa                       <<" time: "<< Real(t3-t2)/cps << std::endl;

}

int
main (int argc, char *argv[])
{


	devel::init( argc, argv );

  using namespace basic::options;
  using namespace basic::options::OptionKeys;
  using namespace utility;

	if( option[ in::file::s ].user() ) {
  	vector1<file::FileName> files( option[ in::file::s ]() );
  	for( size_t i = 1; i <= files.size(); ++i ) {
    	test( files[i] );
  	}
	} else if( option[ in::file::l ].user() ) {
  		vector1<file::FileName> files( option[ in::file::l ]() );
  		for( size_t i = 1; i <= files.size(); ++i ) {
			utility::io::izstream list( files[i] );
			std::string fname;
			while( list >> fname ) {
				// std::cerr << "'" << fname << "'" << std::endl;
    		test( fname );
			}
  		}
	}

	for( std::map<std::string,utility::io::ozstream*>::iterator i = outs.begin(); i != outs.end(); ++i ) {
		i->second->close();
		delete i->second;
	}

	return 0;


}
