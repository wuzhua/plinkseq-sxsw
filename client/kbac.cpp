
#include "pseq.h"
#include "genic.h"

#include <iostream>
#include <functional>
#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <iomanip>
#include <list>


extern GStore g;

using namespace std;




double Pseq::Assoc::Aux_kbac::gw_ln_factorial(const double n) const
{
  /*
    double rlnFact = 0.0;
    if (n == 0 || n == 1) return rlnFact;
    for (int i = 2; i <= n; ++i) rlnFact += log(double(i));
    return rlnFact;
  */

  return lnfact_table[int(n)];
}


double Pseq::Assoc::Aux_kbac::gw_lnchoose( const double n, const double k) const 
{  
  double rlnChoose = 0.0;
  if (n == 0 || n == k || k == 0) return rlnChoose;
  rlnChoose = gw_ln_factorial(n) - gw_ln_factorial(n-k) - gw_ln_factorial(k);
  return rlnChoose;
}



double Pseq::Assoc::Aux_kbac::gw_hypergeometric_pmf(const unsigned int k, 
						    const unsigned int n1, 
						    const unsigned int n2, 
						    const unsigned int t) const 
{
  // prob(k) =  choose(n1, k) choose(n2, t-k) / choose(n1+n2,t)
  
  /*
    if (k > n1 || k > t || t - k > n2 || t > n1 + n2) {
    cout << "error!" << endl; 
    exit(-1);
    }
  */ 
  
  double dn1 = double(n1), dk = double(k), dn2 = double(n2), dt = double(t);
  
  double c1 = gw_lnchoose(dn1,dk);
  double c2 = gw_lnchoose(dn2,dt-dk);
  double c3 = gw_lnchoose(dn1+dn2,dt);
  double rPmf = exp(c1 + c2 - c3);
  
  return rPmf;
}


double Pseq::Assoc::Aux_kbac::gw_hypergeometric_cmf(const unsigned int k, 
						    const unsigned int n1, 
						    const unsigned int n2, 
						    const unsigned int t) const
{
  
  // prob(<= k) = \sum_0^k choose(n1, k) choose(n2, t-k) / choose(n1+n2,t)
    
  /*    
    if (k > n1 || k > t || t - k > n2 || t > n1 + n2) {
    cout << "error!" << endl; 
        exit(-1);
    }
  */
  
  double rCmf = gw_hypergeometric_pmf(k, n1, n2, t);
  double uPmf = rCmf;
  for (double i = k; i >= 0; --i) 
    {
      double diffactor = i / (n1 - i + 1.0) * (n2 - t + i) / (t - i + 1.0);
      uPmf = uPmf * diffactor;
      rCmf += uPmf;
    }  
  return rCmf;
}



//
// KBAC test; Liu et al PLoS Genetics (2010); code by Gao Wang
// Sum of genotype pattern frequencies differences, weighted by hypergeometric kernel.
// Permutation based on 2-sided test
//

double Pseq::Assoc::stat_kbac( const VariantGroup & vars , 
			       Aux_prelim * aux_prelim , 
			       Aux_kbac * aux , 
			       std::map<std::string,std::string> * output ,
			       bool original ) 
  
{

  const int nv = vars.size();  
  const int ni = vars.n_individuals();
  
  // genotype vector -- convert individual genotypes either to a unique ID, or to a unique string. 
  std::vector<std::string> genoStr( ni );
    
  // wild-type genotype
  std::string genoWt;
  
  if ( nv <= 600 ) 
    {
      
      //////
      // compute a genotype pattern (an array of {0, 1, 2, ...}) as a single unique ID score
      // this makes the computation much more speedy
      // due to computational limit it allows only variant sites of length < 646 
      // this is because "pow(3.0, 647) == inf" on many computers
      //////
      
      genoWt = "0";
      
      for (unsigned int smpCnt = 0; smpCnt != ni; ++smpCnt) 
	{
	  
	  double vntId = 0;
	  
	  for (unsigned int locCnt = 0; locCnt != nv; ++locCnt) 
	    { 
	      
	      const int dose = vars(locCnt , smpCnt ).minor_allele_count( aux_prelim->altmin[ locCnt] );
	      
	      if (  dose > 0 ) 
		{
		  // one-to-one "ID" for a genotype pattern
		  vntId += pow(3.0, double(locCnt)) * dose ;
		}                
	      
	    }
	  
	  // convert the ID (which is a number) to a string. 
	  std::stringstream ssVnt;
	  ssVnt << vntId;
	  genoStr[smpCnt] = ssVnt.str(); 
	}
    }
  else 
    {
      
      //////
      // convert a genotype pattern into a string
      // for large regions (length > 646) I cannot find unique ID score for the genotype pattern ... 
      // have to use the string directly
      //////
      
      for (unsigned int smpCnt = 0; smpCnt != ni; ++smpCnt) 
	{
	  for (unsigned int locCnt = 0; locCnt != nv; ++locCnt) 
	    { 
	      const int dose = vars(locCnt,smpCnt).minor_allele_count( aux_prelim->altmin[ locCnt ] );
	      std::stringstream ssVnt;
	      ssVnt << dose;
	      genoStr[smpCnt].append(ssVnt.str()); 
	    }
	}
      
      for (unsigned int locCnt = 0; locCnt != nv; ++locCnt) 
	{ 	  
	  std::stringstream ssVnt;
	  ssVnt << 0;
	  genoWt.append(ssVnt.str());
	}
    }


  
  //////
  // unique genotype patterns
  // use "list" and some STL algorithms
  //////
  
  std::list<std::string> uStr(genoStr.begin(), genoStr.end());
  uStr.remove(genoWt); uStr.sort(); uStr.unique();  
  // remove wildtype and get unique genotype patterns 
  std::vector<std::string> upat(uStr.size());
  copy(uStr.begin(), uStr.end(), upat.begin());
  uStr.clear(); genoWt.clear();
  
  // "vector<string> upat" is unique genotype patterns that occur in the sample  
  
  //////
  // count number of sample individuals for each genotype pattern
  //////
  

  // genotype pattern counts
  std::vector<unsigned int> mulGnCnt(upat.size(),0);



  // for each sample, identify/count its genotype pattern

  for (unsigned int smpCnt = 0; smpCnt != ni; ++smpCnt) 
    {
      
      for (unsigned int uCnt = 0; uCnt != upat.size(); ++uCnt) 
	{
	  
	  // genotype pattern identified
	  if (genoStr[smpCnt] == upat[uCnt]) 
	    {	     
	      // count this genotype pattern
	      ++mulGnCnt[uCnt];	      
	      break;
	    }	  
	}
    }
  
  
//   std::cout << "Number of each unique individual genotype patterns (excluding wildtype): " << "\n" << endl;
//   for (unsigned int smpCnt = 0; smpCnt != upat.size(); ++smpCnt) std::cout << mulGnCnt[smpCnt] << ", "; 
//   cout << "\n" << endl;


  // ydat is phenotype

  
  ////
  // count number of sample cases for each genotype pattern
  ////

  int numCs = 0;

  // genotype pattern counts in cases
  std::vector<unsigned int> mulGnCntCs(upat.size(),0);
    
  for (unsigned int smpCnt = 0; smpCnt != ni; ++smpCnt) 
    {

      // for each "case", identify/count its genotype pattern
      if ( vars.ind( g.perm.pos( smpCnt ) )->affected() == CASE ) 
	{

	  ++numCs;

	  for (unsigned int uCnt = 0; uCnt != upat.size(); ++uCnt) 
	    {
	      
	      // genotype pattern identified in cases
	      if (genoStr[smpCnt] == upat[uCnt]) 
		{		
		  ++mulGnCntCs[uCnt];
		  // count this genotype pattern
		  break;
		}
	      else;
	      // genotype pattern not found -- move on to next pattern
	    }
	}
      else;
    }


  // KBAC weights: hypergeometric distribution cmf
  
  std::vector<double> mulGnWgt( upat.size(), 0 );
  for (unsigned int uCnt = 0; uCnt != upat.size(); ++uCnt) 
    {
      //   mulGnWgt[uCnt] = gsl_cdf_hypergeometric_P(mulGnCntCs[uCnt], mulGnCnt[uCnt], ni - mulGnCnt[uCnt], numCs);
      mulGnWgt[uCnt] = aux->gw_hypergeometric_cmf(mulGnCntCs[uCnt], mulGnCnt[uCnt], ni - mulGnCnt[uCnt], numCs);
    }
  

  // KBAC statistic: sum of genotype pattern frequencies differences
  // in cases vs. controls, weighted by the hypergeometric
  // distribution kernel test is two-sided

  double kbac = 0;
  for (unsigned int uCnt = 0; uCnt != upat.size(); ++uCnt) 
    kbac += ( double(mulGnCntCs[uCnt]) / double(numCs) - double(mulGnCnt[uCnt] - mulGnCntCs[uCnt]) / double(ni - numCs) ) *  mulGnWgt[uCnt];
  
  return kbac * kbac; 

}