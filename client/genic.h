#ifndef __PSEQ_GENIC_H__
#define __PSEQ_GENIC_H__

#include <map>
#include "pseq.h"
#include <cmath>

class VariantGroup;

namespace Pseq 
{
  
  namespace Assoc
  {
    
    struct Aux_prelim;
    struct Aux_burden;
    struct Aux_calpha;
    struct Aux_fw_vt;
    struct Aux_cancor;
    struct Aux_hoffman_witte;
    struct Aux_kbac;
     
    void  prelim( const VariantGroup & vars , Aux_prelim * aux );
    
    void stat_burden( const VariantGroup & vars , 
		      Aux_prelim * pre , 
		      Aux_burden * aux , 
		      std::map<std::string,std::string> * output , 
		      bool original );
    
    double stat_calpha( const VariantGroup & , 
			Aux_prelim * , 
			Aux_calpha * , 
			std::map<std::string,std::string> * ,
			bool );
        
    void stat_fw_vt( const VariantGroup & , 
		     Aux_prelim * , 
		     Aux_fw_vt *  , 
		     std::map<std::string,std::string> *  , 
		     bool original );
    
    void stat_cancor( const VariantGroup & , 
		      Aux_prelim * , 
		      Aux_cancor * , 
		      std::map<std::string,std::string> *  , 
		      bool original );
    
    double stat_hoffman_witte( const VariantGroup & , 
			       Aux_hoffman_witte * , 
			       std::map<std::string,std::string> *  , 
			       bool original );
    
    
    double stat_kbac( const VariantGroup & , 
		      Aux_prelim * , 
		      Aux_kbac * , 
		      std::map<std::string,std::string> *  , 
		      bool original );
    

    struct AuxGenic 
    {
      
      AuxGenic() 
      { 
	g=NULL;
	rseed=0; 
	fix_null_genotypes = true;
	show_info = false; 
	show_midbp = false;
	vanilla = true;
	burden = true;
	site_burden = false;
	uniq = true;
	mhit = false;
	vt = false; 
	fw = false;
	calpha = false;
	cancor = false;
	hoffman_witte = false;
	kbac = false;
      }
      
      int n_tests() const 
      {
	return vanilla 
	  + burden 
	  + uniq 
	  + mhit 
	  + vt 
	  + fw 
	  + calpha 
	  + cancor 
	  + hoffman_witte
	  + kbac;
      }
      
      GStore * g;
      long int rseed;
      bool fix_null_genotypes;
      bool show_info;
      bool show_midbp;

      // genic tests      
      bool mhit;
      bool vanilla;
      bool burden;
      bool site_burden; // modifier, not a test
      bool uniq;
      bool fw;
      bool vt;
      bool calpha;
      bool cancor;
      bool hoffman_witte;
      bool kbac;

    };
 
    
    struct Aux_prelim {
      
      int minm;
      int maxm;

      bool dichot;
      int n_a;
      int n_u;
      int n_t;
	
      std::set<int> refmin;            // track alleles at which reference is minor 
      std::vector<bool> altmin;        // same info but as vector

      std::map<int,int> mc;            // key=#minor alleles,value=#variants   
      std::map<std::string,int> mc_a;  // count in affecteds
      
      std::vector<double> fweights;    // frequency weights
      std::vector<int> acounts;        // allele counts (used in VT)
      
      std::map<int,std::set<int> > carriers; 
      
    };
    

    struct Aux_burden {
      
      Aux_burden( bool vanilla , 
		  bool burden , 		  
		  bool uniq , 
		  bool mhit , 
		  bool site_burden  )
	: vanilla(vanilla) , burden(burden) , 
	  uniq(uniq) , mhit(mhit) , site_burden(false) { } 
      
      bool vanilla;
      bool burden;
      bool site_burden;
      bool uniq;
      bool mhit;
      
      double stat_vanilla;
      double stat_burden;
      double stat_uniq;
      double stat_mhit;
    };


    struct Aux_fw_vt { 
      Aux_fw_vt(bool fw , bool vt) : fw(fw), vt(vt) { } 
      double pmean; // phenotypic mean
      bool fw;
      bool vt;
      double stat_fw;
      double stat_vt;
    };
    

    struct Aux_calpha {
      double p_a;
      double p_ap_u;      
      double variance;
    };


    struct Aux_cancor { 
      Aux_cancor(int np, int ng, int ni) : P(ni,np) , G(ni,ng) { } 
      double stat;
      Data::Matrix<double> P; 
      Data::Matrix<double> G;
    };
    

    struct Aux_hoffman_witte {      
      Aux_hoffman_witte( const VariantGroup & vars, Aux_prelim * p );      
      std::vector<int> piece_begin; // cutpoint begin points
      std::vector<int> piece_end;   // cutpoint end points
      double tbar;                  // mean of the trait
      std::vector<double> xbar;     // mean of the genotypes      
      Aux_prelim * aux_prelim;
      bool dichot;
      
      double calc_stat( const VariantGroup & , 
		      const std::vector<int> & , 
		      const std::vector<double> & , 
		      const std::vector<double> & );
      
      void clear() 
      {
	piece_begin.clear();
	piece_end.clear();
	tbar = 0.0;
	xbar.clear();
      }      
    };

    
    struct Aux_kbac {
      static double lnfact_table[]; 
      double gw_ln_factorial( const double ) const;
      double gw_lnchoose( const double , const double ) const;
      double gw_hypergeometric_pmf(const unsigned int , const unsigned int , const unsigned int , const unsigned int ) const;
      double gw_hypergeometric_cmf(const unsigned int , const unsigned int , const unsigned int , const unsigned int ) const;

      
    };
    

  }  
  
}


#endif
