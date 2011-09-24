// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// This file is part of the Rosetta software suite and is made available under license.
// The Rosetta software is developed by the contributing members of the Rosetta Commons consortium.
// (C) 199x-2009 Rosetta Commons participating institutions and developers.
// For more information, see http://www.rosettacommons.org/.

/// @file /src/apps/pilat/will/crossmatch.cc
/// @brief crosses matches fast

#include <core/scoring/constraints/ResidueTypeConstraint.hh>
#include <core/scoring/hbonds/hbonds.hh>
#include <core/scoring/hbonds/HBondSet.hh>
#include <core/scoring/sasa.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/enzdes.OptionKeys.gen.hh>
#include <numeric/constants.hh>
#include "numeric/xyz.io.hh"

using core::Real;
using core::Size;
using core::scoring::ScoreFunctionOP;
using core::id::AtomID;
typedef numeric::xyzMatrix<Real> Mat;
typedef numeric::xyzVector<Real> Vec;
typedef utility::vector1<Vec>    Vecs;
using numeric::conversions::radians;

bool BB_UNS_INCLUDE_TERMINI = false;


inline Vec projperp(Vec const & u, Vec const & v) {
  return v - projection_matrix(u)*v;
}


void xform_pose( core::pose::Pose & pose, core::kinematics::Stub const & s, Size sres=1, Size eres=0 ) {
  if(eres==0) eres = pose.n_residue();
  for(Size ir = sres; ir <= eres; ++ir) {
    for(Size ia = 1; ia <= pose.residue_type(ir).natoms(); ++ia) {
      core::id::AtomID const aid(core::id::AtomID(ia,ir));
      pose.set_xyz( aid, s.local2global(pose.xyz(aid)) );
    }
  }
}
void xform_pose_rev( core::pose::Pose & pose, core::kinematics::Stub const & s ) {
  for(Size ir = 1; ir <= pose.n_residue(); ++ir) {
    for(Size ia = 1; ia <= pose.residue_type(ir).natoms(); ++ia) {
      core::id::AtomID const aid(core::id::AtomID(ia,ir));
      pose.set_xyz( aid, s.global2local(pose.xyz(aid)) );
    }
  }
}



core::kinematics::Stub getxform(core::conformation::Residue const & move_resi, core::conformation::Residue const & fixd_resi) {
  core::kinematics::Stub s;
  s.M = alignVectorSets(move_resi.xyz(1)-move_resi.xyz(2),move_resi.xyz(3)-move_resi.xyz(2),fixd_resi.xyz(1)-fixd_resi.xyz(2),fixd_resi.xyz(3)-fixd_resi.xyz(2));
  s.v = fixd_resi.xyz(2)-s.M*move_resi.xyz(2);
  return s;
}

void trans_pose( core::pose::Pose & pose, Vec const & trans ) {
  for(Size ir = 1; ir <= pose.n_residue(); ++ir) {
    for(Size ia = 1; ia <= pose.residue_type(ir).natoms(); ++ia) {
      core::id::AtomID const aid(core::id::AtomID(ia,ir));
      pose.set_xyz( aid, pose.xyz(aid) + trans );
    }
  }
}

void rot_pose( core::pose::Pose & pose, Mat const & rot ) {
  for(Size ir = 1; ir <= pose.n_residue(); ++ir) {
    for(Size ia = 1; ia <= pose.residue_type(ir).natoms(); ++ia) {
      core::id::AtomID const aid(core::id::AtomID(ia,ir));
      pose.set_xyz( aid, rot * pose.xyz(aid) );
    }
  }
}

void rot_pose( core::pose::Pose & pose, Mat const & rot, Vec const & cen ) {
  trans_pose(pose,-cen);
  rot_pose(pose,rot);
  trans_pose(pose,cen);
}

void rot_pose( core::pose::Pose & pose, Vec const & axis, Real const & ang ) {
  rot_pose(pose,rotation_matrix_degrees(axis,ang));
}

void rot_pose( core::pose::Pose & pose, Vec const & axis, Real const & ang, Vec const & cen ) {
  rot_pose(pose,rotation_matrix_degrees(axis,ang),cen);
}


Vec com(core::pose::Pose const & pose, Size str, Size end) {
	Vec c(0,0,0);
	for(Size i = str; i <= end; ++i) {
		c += pose.xyz(AtomID(2,i));
	}
	c /= Real(end-str+1);
	return c;
}

Vec symaxis(core::pose::Pose & pose, Size nsub, Size nsym, Size nblk) {
	Size const st = nsub*nsym*nblk;
	Vec c = com(pose,st+1,st+nsym*nsub);
	Vec a(0,0,0);
	for(Size i = 1; i <= nsub; ++i) {
		Vec tmp = Vec(0,0,0);
		for(Size j = 0; j < nsym; ++j) {
			tmp += pose.xyz(AtomID(2,st+i+j*nsub));
		}
		tmp = tmp/(Real)nsym - c;
		if( a.length() > 0.0001 && a.dot(tmp) < 0 ) tmp = -tmp;
		a += tmp;
	}
	return a.normalized();
}

void alignaxis(core::pose::Pose & pose, Vec newaxis, Vec oldaxis, Vec cen = Vec(0,0,0) ) {
  newaxis.normalize();
  oldaxis.normalize();
  Vec axis = newaxis.cross(oldaxis).normalized();
  Real ang = -acos(numeric::max(-1.0,numeric::min(1.0,newaxis.dot(oldaxis))))*180/numeric::constants::d::pi;
  rot_pose(pose,axis,ang,cen);
}


utility::vector1< core::scoring::constraints::ConstraintCOP >
add_favor_native_cst(
                     core::pose::Pose & pose
                     ) {
  using namespace core::scoring::constraints;
  core::Real bonus = basic::options::option[basic::options::OptionKeys::enzdes::favor_native_res].value();
  // std::cout << "favor_native_res: adding a bonus of " << bonus << " for native residues to pose." << std::endl;
  utility::vector1< core::scoring::constraints::ConstraintCOP > favor_native_constraints;
  for( core::Size i = 1; i <= pose.total_residue(); ++i) {
    ConstraintOP resconstraint = new ResidueTypeConstraint( pose, i, bonus );
    favor_native_constraints.push_back( resconstraint );
  }
  return pose.add_constraints( favor_native_constraints );
}


core::id::AtomID_Map<Real>
get_hbonde(
           core::pose::Pose & pose
           ) {
  ScoreFunctionOP sf = core::scoring::getScoreFunction();
  // if(core::pose::symmetry::is_symmetric(pose)) {
  //  sf = new core::scoring::symmetry::SymmetricScoreFunction(sf);
  // }
  sf->score(pose);
  core::id::AtomID_Map<Real> hbond_e;
  core::pose::initialize_atomid_map(hbond_e,pose,0.0);
  sf->score(pose);
  core::scoring::hbonds::HBondSet hbset;
  core::scoring::hbonds::fill_hbond_set( pose, false, hbset, false );
  for( core::Size i = 1; i <= hbset.nhbonds(); ++i ) {
    core::scoring::hbonds::HBond const & hbond( hbset.hbond(i) );
    core::id::AtomID aid = core::id::AtomID(hbond.acc_atm(),hbond.acc_res());
    core::Size iabase = pose.residue(hbond.don_res()).atom_base(hbond.don_hatm());
    core::id::AtomID donatom = core::id::AtomID(hbond.don_hatm(),hbond.don_res());
    core::id::AtomID donparent(iabase,hbond.don_res());
    hbond_e[aid] += hbond.energy();
    hbond_e[donatom] += hbond.energy();
    hbond_e[donparent] += hbond.energy();
  }
  return hbond_e;
}

core::id::AtomID_Map<core::Real> compute_bb_sasa(core::pose::Pose const & pose, Real probe_radius) {
  utility::vector1<Real> rsd_sasa(pose.n_residue(),0.0);
  core::id::AtomID_Map<Real> atom_sasa;
  core::id::AtomID_Map<bool> atom_mask;
  core::pose::initialize_atomid_map(atom_sasa,pose,0.0);
  core::pose::initialize_atomid_map(atom_mask,pose,false);
  Size nres = pose.n_residue();
  // if(core::pose::symmetry::is_symmetric(pose)) {
  //  nres = core::pose::symmetry::symmetry_info(pose)->num_total_residues_without_pseudo();
  // }
  for(Size i = 1; i <= nres; i++) {
    for(Size j = 1; j <= numeric::min(pose.residue(i).nheavyatoms(),(Size)5); j++) {
      atom_mask[AtomID(j,i)] = true;
    }
  }
  core::scoring::calc_per_atom_sasa( pose, atom_sasa, rsd_sasa, probe_radius, true, atom_mask );
  return atom_sasa;
}


utility::vector1< core::id::AtomID >
get_bb_bur_uns(
               core::pose::Pose & pose,
               Real probe_radius=2.0,
               Size start_rsd=1,
               Size end_rsd=0
               ) {
  if( 0 == end_rsd ) end_rsd = pose.n_residue();
  utility::vector1< core::id::AtomID > bb_bur_uns;
  // calc sasa
  utility::vector1<Real> rsd_sasa(pose.n_residue(),0.0);
  core::id::AtomID_Map<Real> atom_sasa;
  core::id::AtomID_Map<bool> atom_mask;
  core::pose::initialize_atomid_map(atom_sasa,pose,0.0);
  core::pose::initialize_atomid_map(atom_mask,pose,false);
  for(Size i = start_rsd; i <= end_rsd; i++) {
    // for(Size j = 1; j <= 5; j++) {
    for(Size j = 1; j <= pose.residue(i).nheavyatoms(); j++) {
      atom_mask[AtomID(j,i)] = true;
    }
    if(pose.residue(i).has("H")) atom_mask[AtomID(pose.residue(i).atom_index("H"),i)] = true;
  }
  core::scoring::calc_per_atom_sasa( pose, atom_sasa, rsd_sasa, probe_radius, true, atom_mask );
  core::id::AtomID_Map<Real> hbond_e = get_hbonde(pose); // could be done outside... small inefficiency ALSO possibly incorrect to compute hbonds for everything...
  for(Size i = start_rsd; i <= end_rsd; ++i) {
    if(!pose.residue(i).is_lower_terminus()) {
      core::id::AtomID aid(pose.residue(i).atom_index("H"),i);
      if( atom_sasa[aid] == 0.0 && hbond_e[aid] == 0.0 ) {
        bb_bur_uns.push_back( aid );
      }
    } else if(BB_UNS_INCLUDE_TERMINI) { // is lower terminus
      core::id::AtomID aid(pose.residue(i).atom_index("N"),i);
      if( atom_sasa[aid] == 0.0 ) {
        bb_bur_uns.push_back(aid);
      }
    }
    if(!pose.residue(i).is_upper_terminus()) {
      core::id::AtomID aid(pose.residue(i).atom_index("O"),i);
      if( atom_sasa[aid] == 0.0 && hbond_e[aid] == 0.0 ) {
        bb_bur_uns.push_back( aid );
      }
    } else if(BB_UNS_INCLUDE_TERMINI) { // is upper terminus
      core::id::AtomID aid1(pose.residue(i).atom_index("O"),i);
      core::id::AtomID aid2(pose.residue(i).atom_index("OXT"),i);
      if( atom_sasa[aid1] == 0.0 ) bb_bur_uns.push_back(aid1);
      if( atom_sasa[aid2] == 0.0 ) bb_bur_uns.push_back(aid2);
    }
  }
  return bb_bur_uns;
}


utility::vector1< core::id::AtomID >
get_bb_bur_uns_iface(
                     core::pose::Pose & pose,
                     utility::vector1<std::pair<Size,Size> > sections,
                     Real probe_radius = 2.0
                     ) {
  utility::vector1< core::id::AtomID > bb_bur_uns_all = get_bb_bur_uns(pose,probe_radius,1,pose.n_residue());
  utility::vector1< utility::vector1< core::id::AtomID > > bb_bur_uns_sec;
  for(utility::vector1<std::pair<Size,Size> >::iterator isec = sections.begin(); isec != sections.end(); ++isec) {
    Size start_rsd = isec->first;
    Size   end_rsd = isec->second;
    bb_bur_uns_sec.push_back( get_bb_bur_uns(pose,probe_radius,start_rsd,end_rsd) );
  }
  utility::vector1< core::id::AtomID > bb_bur_uns_iface;
  for(utility::vector1< core::id::AtomID >::iterator i = bb_bur_uns_all.begin(); i != bb_bur_uns_all.end(); ++i) {
    bool on_iface = true;
    for(utility::vector1< utility::vector1< core::id::AtomID > >::iterator j = bb_bur_uns_sec.begin(); j != bb_bur_uns_sec.end(); ++j) {
      if( std::find(j->begin(),j->end(),*i) != j->end() ) {
        on_iface = false;
      }
    }
    if(on_iface) bb_bur_uns_iface.push_back(*i);
  }
  return bb_bur_uns_iface;
}


template< typename T >
inline
std::string
str( T const & t )
{
	return ObjexxFCL::string_of(t);
}

template< typename T >
inline
std::string
lzs(
	T const & t,
	int const w // Minimum width
)
{
	return ObjexxFCL::lead_zero_string_of(t,w);
}



Size pose_natom(core::pose::Pose const& p){
  Size natom = 0;
  for(int ir = 1; ir <= p.n_residue(); ++ir) natom += p.residue(ir).nheavyatoms();
  return natom;
}

void remove_termini(core::pose::Pose & p) {
  for(Size i = 1; i <= p.n_residue(); ++i) {
    if(p.residue(i).is_lower_terminus()) remove_lower_terminus_type_from_pose_residue(p,i);
    if(p.residue(i).is_upper_terminus()) remove_upper_terminus_type_from_pose_residue(p,i);
  }
}

Vec safe_xyz(core::pose::Pose const & p, int ano, int rsd) {
  if(rsd<1 || rsd>p.n_residue()) return Vec(NAN,NAN,NAN);
  return p.xyz(AtomID(ano,rsd));
}

Vec orb_xyz(core::pose::Pose const & p, int oi, int rsd) {
  if(rsd<1 || rsd>p.n_residue()) return Vec(NAN,NAN,NAN);
  return p.residue(rsd).orbital_xyz(oi);
}

void to_canonical_bb_frame(core::pose::Pose & pose) {
	core::conformation::Residue const & r(pose.residue(1));
	if(!r.has( "N")) utility_exit_with_message("to_canonical_frame: res must have N");
	if(!r.has("CA")) utility_exit_with_message("to_canonical_frame: res must have CA");
	if(!r.has( "C")) utility_exit_with_message("to_canonical_frame: res must have C");
	Vec  n = r.xyz( "N");
	Vec ca = r.xyz("CA");
	Vec  c = r.xyz( "C");
	Vec X0 = (n-ca).normalized();
	Vec Y0 = projperp(X0,(c-ca)).normalized();
	Vec Z0 = X0.cross(Y0);
	rot_pose(pose,Mat::rows(X0,Y0,Z0));
	trans_pose(pose,-r.xyz("CA"));
	{
		Vec n = r.xyz("N");
		Vec ca = r.xyz("CA");
		Vec c = r.xyz("C");
		Vec X0 = (n-ca).normalized();
		Vec Y0 = projperp(X0,(c-ca)).normalized();
		Vec Z0 = X0.cross(Y0);
		std::cout << " N " << X0 << std::endl;
		std::cout << "CA " << Y0 << std::endl;
		std::cout << " C " << Z0 << std::endl;
	}
}

void to_canonical_sc_frame(core::pose::Pose & pose) {
	core::conformation::Residue const & r(pose.residue(1));
	if(!(r.has("CG")||r.has("SG"))) utility_exit_with_message("to_canonical_frame: res must have CG/SG");
	if(!r.has("CB")) utility_exit_with_message("to_canonical_frame: res must have CB");
	if(!r.has("CA")) utility_exit_with_message("to_canonical_frame: res must have CA");
	Vec cg = r.has("CG") ? r.xyz("CG") : r.xyz("SG");
	Vec cb = r.xyz("CB");
	Vec ca = r.xyz("CA");
	Vec Z0 = (cg-cb).normalized();
	Vec X0 = projperp(Z0,(ca-cb)).normalized();
	Vec Y0 = Z0.cross(X0);
	rot_pose(pose,Mat::rows(X0,Y0,Z0));
	trans_pose(pose,-r.xyz("CB"));
	// {
	// 	Vec n = r.xyz("N");
	// 	Vec ca = r.xyz("CA");
	// 	Vec c = r.xyz("C");
	// 	Vec X0 = (n-ca).normalized();
	// 	Vec Y0 = projperp(X0,(c-ca)).normalized();
	// 	Vec Z0 = X0.cross(Y0);
	// 	std::cout << " N " <<  n << std::endl;
	// 	std::cout << "CA " << ca << std::endl;
	// 	std::cout << " C " <<  c << std::endl;
	// 	std::cout << " X " << X0 << std::endl;
	// 	std::cout << " Y " << Y0 << std::endl;
	// 	std::cout << " Z " << Z0 << std::endl;
	// }
}

void to_canonical_sc_frame_from_bb(core::pose::Pose & pose) {
	core::conformation::Residue const & r(pose.residue(1));
	Vec n =  r.xyz("N");
	Vec ca = r.xyz("CA");
	Vec c =  r.xyz("C");
	Vec X0 = (n-ca).normalized();
	Vec Y0 = projperp(X0,(c-ca)).normalized();
	Vec Z0 = X0.cross(Y0);
	Mat fr = Mat::rows(X0,Y0,Z0);
	Vec X1(  0.6876640316337246,0.0000000000000000, 0.7260290487282527); // from above,
	Vec Y1(  0.3873006764891879,0.8458311837980901,-0.3668348327323059); // only for
	Vec Z1( -0.6140980097576192,0.5334506617436343, 0.5816476819321206); // homogeneous res!
	Mat to = Mat::cols(X1,Y1,Z1);
	rot_pose(pose,to*fr);
	trans_pose(pose,Vec(1.408444490011426,0,-0.5963368684732249) - r.xyz("CA"));
}

std::string
strip(std::string StringToModify) {
   if(StringToModify.empty()) return "";
   int startIndex = StringToModify.find_first_not_of(" ");
   int endIndex = StringToModify.find_last_not_of(" ");
	 std::string tempString = StringToModify;
   return tempString.substr(startIndex, (endIndex-startIndex+ 1) );
}
