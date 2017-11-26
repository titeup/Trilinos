// @HEADER
// ************************************************************************
//
//               Rapid Optimization Library (ROL) Package
//                 Copyright (2014) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact lead developers:
//              Drew Kouri   (dpkouri@sandia.gov) and
//              Denis Ridzal (dridzal@sandia.gov)
//
// ************************************************************************
// @HEADER


#ifndef ROL_MERITFUNCTION_H
#define ROL_MERITFUNCTION_H

#include "ROL_Objective.hpp"
#include "ROL_InequalityConstraint.hpp"
#include "ROL_PartitionedVector.hpp"

/*  Nonsmooth merit function as depicted in Eq. 19.36 of Nocedal and Wright Second Edition

 \f[
 \phi_\nu(x,s) = f(x) - \mu\sum\limits_{i=1}^m \ln(s_i) 
        + \nu \| c_E(x)\| + \nu + \| c_I(x)-s\|  
 \f] 

 using the Euclidean norm without squares
 */


namespace ROL {
namespace InteriorPoint {

template<class Real>
class MeritFunction : public Objective<Real> {

  typedef Vector<Real>                V;
  typedef PartitionedVector<Real>     PV;
  typedef Objective<Real>             OBJ;
  typedef Constraint<Real>    EQCON;
  typedef InequalityConstraint<Real>  INCON;

  typedef Teuchos::ParameterList      PLIST;


  typedef typename PV::size_type      uint;

  const static uint OPT   = 0;
  const static uint SLACK = 1; 


private:

  ROL::SharedPointer<OBJ>   obj_;       // Raw objective function
  ROL::SharedPointer<EQCON> eqcon_;     //  constraint
  ROL::SharedPointer<INCON> incon_;     // Inequality constraint
  ROL::SharedPointer<BND>   bnd_;       // Bound constraint

  Real mu_;                       // Penalty parameter for log barrier on slack
  Real nu_;                       // Penalty parameter for constraint norms

  ROL::SharedPointer<OBJ>    obj_;
  ROL::SharedPointer<EQCON>  eqcon_;
  ROL::SharedPointer<INCON>  incon_;

  ROL::SharedPointer<V> xopt_;
  ROL::SharedPointer<V> slack_;

  ROL::SharedPointer<V> gopt_;          // Gradient of the objective function

  ROL::SharedPointer<V> sfun_;          // store elementwise function of slack variable


  ROL::SharedPointer<V> eqmult_;        //  constraint Lagrange multiplier 
  ROL::SharedPointer<V> inmult_;        // Inequality constraint Lagrange multiplier

  ROL::SharedPointer<V> ce_;            //  constraint vector
  ROL::SharedPointer<V> ci_;            // Inequation constraint vector

  ROL::SharedPointer<V> jced_;          //  Jacobian applied to d
  ROL::SharedPointer<V> jcid_;          // Inequality Jacobian applied to d 

  Real cenorm_;
  Real cinorm_;


  static const Elementwise::Logarithm<Real>    LOG_;
  static const Elementwise::Reciprocal<Real>   RECIP_;
  static const Elementwise::ReductionSum<Real> SUM_;


public:

  MeritFunction( ROL::SharedPointer<OBJ>   &obj, 
                 ROL::SharedPointer<EQCON> &eqcon,
                 ROL::SharedPointer<INCON> &incon,
                 const V& x,
                 const V& eqmult,
                 const V& inmult,
                 PLIST &parlist ) :
                 obj_(obj), eqcon_(eqcon), incon_(incon) {

    const PV &xpv = dynamic_cast<const PV&>(x);
    xopt_  = xpv.get(OPT);
    slack_ = xpv.get(SLACK);     
    sfun_  = slack_->clone();

    gopt_  = xopt_->dual().clone();

    PLIST &iplist = parlist.sublist("Step").sublist("Primal-Dual Interior Point");
    mu_ = iplist.get("Initial Slack Penalty");
    nu_ = iplist.get("Initial Constraint Norm Penalty");  

  }


  Real value( const V &x, Real &tol ) {

    const PV &xpv = dynamic_cast<const PV&>(x);
    xopt_  = xpv.get(OPT);
    slack_ = xpv.get(SLACK);

    sfun_->set(*slack_);
 
    sfun_->applyUnary(LOG_);

    Real val = obj_->value(*xopt_,tol);    

    val += mu_*logs_->reduce(SUM_);

    eqcon_->value(*ce_,*xopt_,tol);
    incon_->value(*ci_,*xopt_,tol);

    cenorm_ = ce_->norm();
    cinorm_ = ci_->norm(); 

    val += nu_*(cenorm_ + cinorm_);     
 
    return val;
  }


  Real dirDeriv( const V &x, const V &d, Real tol ) {
   
    const PV &xpv = dynamic_cast<const PV&>(x);
    xopt_  = xpv.get(OPT);
    slack_ = xpv.get(SLACK);

    const PV &dpv = dynamic_cast<const PV&>(d);
    ROL::SharedPointer<V> dopt   = dpv.get(OPT);
    ROL::SharedPointer<V> dslack = dpv.get(SLACK);

    sfun_->set(*slack);
    sfun_->applyUnary(RECIP_);

    ce_->applyJacobian(*jced_,*dopt,*xopt,tol);
    ci_->applyJacobian(*jcid_,*dopt,*xopt,tol);
 
    obj_->gradient(*gopt_,*xopt,tol);    


    // Contributions to directional derivatives
    Real ddopt = gopt_->dot(*dopt);

    Real ddslack = sfun_->dot(*dslack);

    Real ddce = ce_->dot(*jced_)/cenorm_;    

    Real ddci = ci_->dot(*jcid_)/cinorm_;    
   
    Real ddsn = slack_->dot(*dslack)/slack->norm();
 
    return ddopt - mu_*ddslack + nu_*(ddce + ddci + ddsn);  

  }


  void updateBarrier( Real mu ) {
    mu_ = mu;
  }



}; // class MeritFunction

} // namespace InteriorPoint
} // namespace ROL


#endif // ROL_MERITFUNCTION_H











