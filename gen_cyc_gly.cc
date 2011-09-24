// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// This file is part of the Rosetta software suite and is made available under license.
// The Rosetta software is developed by the contributing members of the Rosetta Commons consortium.
// (C) 199x-2009 Rosetta Commons participating institutions and developers.
// For more information, see http://www.rosettacommons.org/.

/// @file /src/apps/pilat/will/rblinker.cc
/// @brief samples rigid bodies connected by linker

#include <basic/options/option.hh>
#include <basic/options/keys/cyclic.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <core/chemical/AtomType.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/util.hh>
#include <core/chemical/VariantType.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/id/DOF_ID.hh>
#include <core/io/silent/ScoreFileSilentStruct.hh>
#include <core/io/silent/SilentFileData.hh>
#include <core/init.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/pose/annotated_sequence.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/scoring/constraints/AtomPairConstraint.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/ScoringManager.hh>
#include <basic/Tracer.hh>
#include <numeric/random/random.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/xyz.io.hh>
#include <ObjexxFCL/format.hh>
#include <ObjexxFCL/string.functions.hh>
#include <protocols/moves/MinMover.hh>
#include <protocols/moves/MonteCarlo.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/moves/RepeatMover.hh>
#include <protocols/moves/TrialMover.hh>
#include <protocols/toolbox/SwitchResidueTypeSet.hh>
#include <sstream>
#include <utility/io/izstream.hh>
#include <utility/io/ozstream.hh>

using core::Real;
using core::Size;
using core::id::AtomID;
using core::pose::Pose;
using core::scoring::ScoreFunctionOP;

struct AbsFunc : public core::scoring::constraints::Func {
	AbsFunc( Real const x0_in, Real const sd_in ): x0_( x0_in ), sd_( sd_in ){}
	core::scoring::constraints::FuncOP
	clone() const { return new AbsFunc( *this ); }
	Real func( Real const x ) const {
		Real const z = ( x-x0_ )/sd_;
		if(z < 0) return -z;
		else return z;
	}
	Real dfunc( Real const x ) const {
		if(x-x0_ < 0) return -1.0/sd_;
		else return 1.0/sd_;
	}
	void read_data( std::istream & in ){ in >> x0_ >> sd_;  }
	void show_definition( std::ostream &out ) const { out << "ABS " << x0_ << " " << sd_ << std::endl; }
	Real x0() const { return x0_; }  
	Real sd() const { return sd_; }
	void x0( Real x ) { x0_ = x; }
	void sd( Real sd ) { sd_ = sd; }
private:
	Real x0_;
	Real sd_;
};


Real mod360(Real x) {
	while(x >  180.0) x -= 360.0;
	while(x < -180.0) x += 360.0;	
	return x;
}

class CycBBMover : public protocols::moves::Mover {
	Size nres_;
	Size copyres_;
	Real mag_;
public:
	CycBBMover(Pose const & pose, Real mag) : nres_(pose.n_residue()-2),copyres_(pose.n_residue()-1),mag_(mag) {}
	void apply(core::pose::Pose & pose) {
		Size i = std::ceil(numeric::random::uniform()*nres_);
		if(     numeric::random::uniform()<0.5) pose.set_phi(i,pose.phi(i)+numeric::random::gaussian()*mag_);
		else                                    pose.set_psi(i,pose.psi(i)+numeric::random::gaussian()*mag_);
		// if(     numeric::random::uniform()<0.45) pose.set_phi(i,pose.phi(i)+numeric::random::gaussian()*mag_);
		// else if(numeric::random::uniform()<0.90) pose.set_psi(i,pose.psi(i)+numeric::random::gaussian()*mag_);
		// else                                     pose.set_omega(i,pose.omega(i)+numeric::random::gaussian()*mag_/10.0);
		// if     (numeric::random::uniform()<0.495) pose.set_phi(i,pose.phi(i)+numeric::random::gaussian()*mag_);
		// else if(numeric::random::uniform()<0.990) pose.set_psi(i,pose.psi(i)+numeric::random::gaussian()*mag_);
		// else                                      pose.set_omega(i,mod360(pose.psi(i)+180.0));
		// make sure end res is identical
		if( 1 == i ) {
			pose.set_phi(copyres_,pose.phi(1));
			pose.set_psi(copyres_,pose.psi(1));
		}
		if( 2 == i ) {
			pose.set_phi(copyres_+1,pose.phi(1));
			pose.set_psi(copyres_+1,pose.psi(1));
		}
	}
	std::string get_name() const { return "CycBBMover"; }
};


void bb_sample(Pose & pose, ScoreFunctionOP sf, Size niter) {
	protocols::moves::MoverOP bbmove = new CycBBMover(pose,10.0);
	protocols::moves::MonteCarloOP mc = new protocols::moves::MonteCarlo( pose, *sf, 2.0 );
	mc->set_autotemp( true, 2.0 );
	mc->set_temperature( 2.0 );
	protocols::moves::RepeatMover( new protocols::moves::TrialMover(bbmove,mc), niter ).apply( pose );
}

void minimize(Pose & pose, ScoreFunctionOP sf) {
	core::kinematics::MoveMapOP movemap = new core::kinematics::MoveMap;
	movemap->set_bb(true);
	movemap->set_chi(true);
	movemap->set_jump(true);
	protocols::moves::MinMover m( movemap, sf, "dfpmin_armijo_nonmonotone", 1e-5, true, false, false );
	m.apply(pose);
}

Pose cyclic_perm(Pose const & orig, Size start) {
	Pose pose;
	pose.append_residue_by_jump(orig.residue(start),1);
	for(Size i = 1; i <= orig.n_residue()-1; ++i) {
		// std::cout << "appending res " << (i+start-1)%orig.n_residue()+1 << std::endl;
		pose.append_residue_by_bond(orig.residue((start+i-1)%orig.n_residue()+1));
	}
	return pose;
}

Real cyclic_all_atom_rms(Pose const & pose, Pose const & other) {
	Real mr = 9e9;
	for(Size i = 1; i <= pose.n_residue(); ++i) {
		Real r = core::scoring::all_atom_rmsd( cyclic_perm(pose,i), other );
		if( r < mr ) mr = r;
	}
	return mr;
}

void cyclic_superimpose(Pose & move, Pose const & ref) {
	Real mr = 9e9;
	Size am = 0;
	for(Size i = 1; i <= move.n_residue(); ++i) {
		Real r = core::scoring::CA_rmsd( cyclic_perm(move,i), ref );
		if( r < mr ) {
			mr = r;
			am = i;
		}
	}
	move = cyclic_perm(move,am);
	core::scoring::calpha_superimpose_pose(move,ref);
}

int main( int argc, char * argv [] ) {

	using basic::options::option;
	using namespace basic::options::OptionKeys;
	using namespace core::scoring::constraints;

	core::init(argc,argv);

	std::string seq = "G"; while((int)seq.size() < option[cyclic::nres]()) seq += "G";

	// score functions
	ScoreFunctionOP sf = core::scoring::ScoreFunctionFactory::create_score_function( "score3" );
	                sf->set_weight(core::scoring::rama,1.0);
	                sf->set_weight(core::scoring::omega,1.0);
	ScoreFunctionOP sfc = core::scoring::ScoreFunctionFactory::create_score_function( "score3" );
	                sfc->set_weight(core::scoring::rama,1.0);
	                sfc->set_weight(core::scoring::atom_pair_constraint,10.0);
	                sfc->set_weight(core::scoring::omega,1.0);
	ScoreFunctionOP sffa = core::scoring::ScoreFunctionFactory::create_score_function("standard");
	                sffa->set_weight(core::scoring::atom_pair_constraint,10.0);
	                sffa->set_weight(core::scoring::omega,1.0);
	ScoreFunctionOP sffastd = core::scoring::ScoreFunctionFactory::create_score_function("standard");
	                sffastd->set_weight(core::scoring::omega,1.0);

	core::io::silent::SilentFileData sfd;	
	Pose ref;

	for(Size ITER = 1; ITER <= (Size)option[out::nstruct](); ++ITER) {
		while(true) {

			// setup pose
			Pose pose;
			core::pose::make_pose_from_sequence(pose,seq,"centroid",false);
			core::pose::add_variant_type_to_pose_residue(pose,"CUTPOINT_UPPER",       1        );
			core::pose::add_variant_type_to_pose_residue(pose,"CUTPOINT_LOWER",pose.n_residue());
			pose.conformation().declare_chemical_bond( 1, "N", pose.n_residue(), "C" );	
			// Size const nbb( lower_rsd.mainchain_atoms().size() );
			// total_dev +=
			// 	( upper_rsd.atom( upper_rsd.mainchain_atoms()[  1] ).xyz().distance_squared( lower_rsd.atom( "OVL1" ).xyz() ) +
			// 	  upper_rsd.atom( upper_rsd.mainchain_atoms()[  2] ).xyz().distance_squared( lower_rsd.atom( "OVL2" ).xyz() ) +
			// 	  lower_rsd.atom( lower_rsd.mainchain_atoms()[nbb] ).xyz().distance_squared( upper_rsd.atom( "OVU1" ).xyz() ) );

			{
				AtomID a1( pose.residue(1).atom_index(   "N"), 1 ), a2( pose.residue(pose.n_residue()).atom_index("OVL1"), pose.n_residue() );
				AtomID b1( pose.residue(1).atom_index(  "CA"), 1 ), b2( pose.residue(pose.n_residue()).atom_index("OVL2"), pose.n_residue() );
				AtomID c1( pose.residue(1).atom_index("OVU1"), 1 ), c2( pose.residue(pose.n_residue()).atom_index(   "C"), pose.n_residue() );
				pose.add_constraint(new AtomPairConstraint(a1,a2,new AbsFunc(0.0,0.01)));
				pose.add_constraint(new AtomPairConstraint(b1,b2,new AbsFunc(0.0,0.01)));
				pose.add_constraint(new AtomPairConstraint(c1,c2,new AbsFunc(0.0,0.01)));
			}
			// gen structure
			for(Size i = 1; i <= pose.n_residue(); ++i) {
				if(numeric::random::uniform() < 0.0) pose.set_omega(i,  0.0);
				else                                 pose.set_omega(i,180.0);
			}
			bb_sample(pose,sf ,100);
			bb_sample(pose,sfc,1000);

			core::pose::remove_variant_type_from_pose_residue(pose,"CUTPOINT_UPPER",1);
			core::pose::remove_variant_type_from_pose_residue(pose,"CUTPOINT_LOWER",pose.n_residue());			
			protocols::toolbox::switch_to_residue_type_set(pose,"fa_standard");
			core::pose::add_variant_type_to_pose_residue(pose,"CUTPOINT_UPPER",       1        );
			core::pose::add_variant_type_to_pose_residue(pose,"CUTPOINT_LOWER",pose.n_residue());
			{
				AtomID a1( pose.residue(1).atom_index(   "N"), 1 ), a2( pose.residue(pose.n_residue()).atom_index("OVL1"), pose.n_residue() );
				AtomID b1( pose.residue(1).atom_index(  "CA"), 1 ), b2( pose.residue(pose.n_residue()).atom_index("OVL2"), pose.n_residue() );
				AtomID c1( pose.residue(1).atom_index("OVU1"), 1 ), c2( pose.residue(pose.n_residue()).atom_index(   "C"), pose.n_residue() );
				pose.remove_constraints();
				pose.add_constraint(new AtomPairConstraint(a1,a2,new AbsFunc(0.0,0.01)));
				pose.add_constraint(new AtomPairConstraint(b1,b2,new AbsFunc(0.0,0.01)));
				pose.add_constraint(new AtomPairConstraint(c1,c2,new AbsFunc(0.0,0.01)));
			}
			minimize(pose,sffa);

			// sffa->show(pose);
 
			//std::cout << "FAREP " << pose.energies().total_energies()[core::scoring::fa_rep] << std::endl;

			// bool omegafail = false;
			// for(Size i = 2; i < pose.n_residue(); ++i) {
			// 	//std::cout << pose.omega(i) << std::endl;
			// 	if( -200 > pose.omega(i) || (pose.omega(i) > -160 && pose.omega(i) < 160) || 200 < pose.omega(i) ) {
			// 		std::cout << "retry omega " << pose.omega(i) << std::endl;
			// 		omegafail = true;
			// 		break;
			// 	}
			// }
			// if(omegafail) continue;
			if( (*sffastd)(pose) / (pose.n_residue()) > -1.0 ) {
				// std::cout << "score fail: " << (*sffastd)(pose) << std::endl;
				continue;
			}
			// bool clashfail = false;
			// for(Size i = 1; i <= pose.n_residue()-2; ++i) {
			// 	for(Size j = i+1; j <= pose.n_residue()-2; ++j) {
			// 		if(pose.residue(i).xyz(4).distance(pose.residue(j).xyz(4)) < 2.8) {
			// 			std::cout << "clashfail " << i << " " << j << std::endl;
			// 			clashfail = true;
			// 		}
			// 	}
			// }
			// if(clashfail){
			// 	continue;
			// }

			core::io::silent::SilentStructOP ss_out( new core::io::silent::ScoreFileSilentStruct );
			std::string fn = "cyc_gly_"+ObjexxFCL::string_of(pose.n_residue())+"_"+ObjexxFCL::string_of(ITER)+".pdb";
			ss_out->fill_struct(pose,fn);		
			sfd.write_silent_struct( *ss_out, option[ out::file::silent ]() );

			std::cout << "finish " << ITER << " " << pose.energies().total_energy() << std::endl;

			if(ITER==1) ref = pose;
			else cyclic_superimpose(pose,ref);

			pose.dump_pdb(fn);


			break;

		}
	}
}

