#include "plinkseq/vgroup.h"
#include "plinkseq/gstore.h"

using namespace std;
using namespace Helper;

extern GStore * GP;

void VariantGroup::force_add(Variant & v)
{
  vars.push_back(v);
}


void VariantGroup::add(Variant &v ) 
{ 
  
  if ( is_complete ) return;
  
  // If the first variant added, will obviously be okay
  
  if ( vars.size() == 0 ) 
    {		      
      gname = v.meta.get1_string( PLINKSeq::META_GROUP() );
      
      if ( mask.named_grouping() && gname == "" ) 
	is_complete = true;
      vars.push_back(v);
      return;
    }
  
  // If a named grouping has been specified, does this
  // new variant conform?
  
  if ( mask.named_grouping() )
    {
      if ( gname != v.meta.get1_string( PLINKSeq::META_GROUP() ) )
	{
	  is_complete = true;
	  return;
	}
      vars.push_back(v);
      return;
    }

  // in all-group mode, always add
  if ( mask.all_grouping() )
    {
      vars.push_back(v);
      return;
    }

  // We should not have reached here, as some grouping function 
  // should have been specified in the mask in order to use 
  // a VariantGroup. 
  
  is_complete = true;
  
  return;
  
}


int VariantGroup::n_individuals() const
{
  return size() > 0 ? vars[0].size() : 0;
}


void VariantGroup::clear(Variant & v)
{ 

  vars.clear();
  vars.push_back(v);
  
  gname = v.meta.get1_string( PLINKSeq::META_GROUP() );

  if ( mask.named_grouping() && gname == "" )
    is_complete = true;
  else
    is_complete = false;
  
  return;
}


void VariantGroup::clear()
{ 
  vars.clear(); 
  gname = "";
  is_complete = false;
}

std::string VariantGroup::dump( bool vmeta , 
				bool vexpand , 
				bool geno , 
				bool gmeta , 
				bool transpose , 
				bool rarelist , 
				bool show_phenotype , 
				bool only_minor )
{
  
  std::stringstream ss;
  
  // Overall group details
  
  ss << "NAME=[" << name() << "]\t" 
     << "N(V)=" << size() << "\t"
     << "N(I)=" << n_individuals() << "\t"
     << "ALIAS=[" << GP->locdb.alias( name() , false ) << "]\n";
  
  if ( size() == 0 ) 
    return ss.str();
  
  //
  // Rare list
  //

  if ( rarelist ) 
    {
      
      std::map<int,std::string> istr;
      std::map<int,std::string> vstr;
      
      std::map<int,int> icnt;
      std::map<int,int> vcnt;

      for (int i=0; i < size(); i++)
	{

	  // Get the minor allele
	  bool altmin = var(i).n_minor_allele( );
	  
	  for (int j=0; j < n_individuals(); j++ )
	    {	      
	      const Genotype & g = (*this)(i,j);
	      
	      if ( g.minor_allele( altmin ) ) 
		{
		  
		  if ( istr[j] != "" ) istr[j] += ",";
		  istr[j] += var(i).coordinate() + ";" + var(i).geno_label( (*this)(i,j) );
		  
		  if ( vstr[i] != "" ) vstr[i] += ",";
		  vstr[i] += ind(j)->id() + ";" + var(i).geno_label( (*this)(i,j) );
		  
		  ++icnt[j];
		  ++vcnt[i];
		  
		  if ( gmeta ) 
		    {
		      std::stringstream s2;
		      s2 << (*this)(i,j).meta; 
		      istr[j] += ";" + s2.str(); 
		      vstr[i] += ";" + s2.str(); 
		    }
		}
	    }
	}


      //
      // Create rarelist display
      //
      
      std::map<int,std::string>::iterator ii = istr.begin();
      while ( ii != istr.end() )
	{
	  ss << "I\t" 
	     << name() << "\t"	 
	     << ind( ii->first )->id() << "\t";

	  if ( show_phenotype )
	    {
	      if ( GP->phmap.type() == PHE_DICHOT )
		{
		  if ( ind( ii->first )->affected() == CASE ) ss << "CASE\t";
		  else if ( ind( ii->first)->affected() == CONTROL ) ss << "CONTROL\t";
		  else ss << ".\t";
		}
	    }

	  
	  ss << icnt[ ii->first ] << "\t"
	     << "\t"
	     << ii->second 
	     << "\n";	    
	  ++ii;
	}

      std::map<int,std::string>::iterator iv = vstr.begin();
      while ( iv != vstr.end() )
	{
	  ss << "V\t" 
	     << name() << "\t"
	     << var( iv->first ) << "\t"
	     << var( iv->first ).reference() << "\t"
	     << vcnt[ iv->first ] << "\t"
	     << iv->second 
	     << "\n";	    
	  ++iv;
	}
      
      return ss.str();
    }
  
  

  //
  // Per-variant summary
  //
  
  for (int i=0; i< size(); i++)
    {
      
      if ( ! ( transpose && !vmeta ) )
	{
	  ss << "V" << i+1 << "\t";

	  ss << var(i) << "\t";
	  
	  // what about sample meta-information?
	  
	  if ( vmeta ) 
	    {
	      if ( vexpand ) 
		ss << "\n" << var(i).meta.display();
	      else
		ss << var(i).meta;   
	    }
	  ss << "\n";
	}
    }

  if ( ! geno ) return ss.str();
  
  ss << "\n";




  //
  // Show genotypes 
  //

  if ( transpose ) 
    {
      // show var x ind
      
      // indiv. header line

      ss << "V";
      for (int j=0; j < n_individuals(); j++ )
	ss << "\t" << ind(j)->id();	
      ss << "\n";
      
      // Variant rows
      for (int i=0; i< size(); i++)
	{	    

	  ss << var(i) << "\t" << var(i).reference();

	  for (int j=0; j < n_individuals(); j++ )
	    {
	      ss << "\t" << var(i).geno_label( (*this)(i,j) ) ;
	      if ( gmeta ) 
		ss << " " << (*this)(i,j).meta;
	    }
	  ss << "\n";
	}
    }
  else
    {
      // ind x var

      // var. header line

      ss << "I";
      if ( show_phenotype ) ss << "\tPHE";
      for (int i=0; i< size(); i++)
	ss << "\tV" << i+1;

      ss << "\n";
      
      // Get minor alleles, if needed
      std::vector<bool> altmin( size() );
      if ( only_minor )
	{
	  for (int i=0; i<size(); i++)
	    altmin[i] = var(i).n_minor_allele( );
	}

      // Indiv rows
      for (int j=0; j < n_individuals(); j++ )
	{ 
	  // perhaps we skip this person?
	  if ( only_minor ) 
	    {
	      bool minor = false;
	      for (int i=0; i< size(); i++)
		{
		  if ( (*this)(i,j).minor_allele_count( altmin[i] ) ) { minor = true; break; }
		}
	      if ( !minor ) continue;
	    }

	  ss << ind(j)->id();

	  if ( show_phenotype )
	    {
	      if ( GP->phmap.type() == PHE_DICHOT )
		{
		  if ( ind( j )->affected() == CASE ) ss << "\tCASE";
		  else if ( ind( j )->affected() == CONTROL ) ss << "\tCONTROL";
		  else ss << "\t.";
		}
	      else if ( GP->phmap.type() == PHE_QT ) 
		ss << "\t" << ind( j )->qt();
	    }

	  for (int i=0; i< size(); i++)
	    {	    
	      ss << "\t" << var(i).geno_label( (*this)(i,j) ) ;
	      if ( gmeta ) 
		ss << " " << (*this)(i,j).meta;
	    }
	  ss << "\n";
	}
    }
      
  ss << "\n";
  
  return ss.str();
}



std::string VariantGroup::coordinate() const 
{
  if ( size() == 0 ) return "NA";
  int chr = var(0).chromosome();
  int bpmin = var(0).position();
  int bpmax = var(0).stop();
  for (int i=1; i<size(); i++)
    {
      if ( var(i).chromosome() != chr ) return "NA";
      if ( var(i).position() < bpmin ) 
	bpmin = var(i).position();
      if ( var(i).stop() > bpmax ) 
	bpmax = var(i).stop();      
    }
  return Helper::chrCode( chr ) + ":" + Helper::int2str( bpmin ) + ".." + Helper::int2str( bpmax ); 
}

int VariantGroup::midposition() const
{
  if ( size() == 0 ) return -1;
  int chr = var(0).chromosome();
  int bpmin = var(0).position();
  int bpmax = var(0).stop();
  for (int i=1; i<size(); i++)
    {
      if ( var(i).chromosome() != chr ) return -1;
      if ( var(i).position() < bpmin ) 
	bpmin = var(i).position();
      if ( var(i).stop() > bpmax ) 
	bpmax = var(i).stop();      
    }
  
  return (int)(bpmin + (bpmax-bpmin)/2 );
  
}

bool VariantGroup::location( int * chr , int * bp1 , int * bp2 ) const
{
  if ( chr == NULL || bp1 == NULL || bp2 == NULL ) return false;
  if ( size() == 0 ) return false;
  *chr = var(0).chromosome();
  *bp1 = var(0).position();
  *bp2 = var(0).stop();
  for (int i=1; i<size(); i++)
    {
      if ( var(i).chromosome() != *chr ) { *chr=0; return false; }
      if ( var(i).position() < *bp1 ) 
	*bp1 = var(i).position();
      if ( var(i).stop() > *bp2 ) 
	*bp2 = var(i).stop();
    }
  return true;
}

int VariantGroup::span() const
{
  if ( size() == 0 ) return -1;
  int chr = var(0).chromosome();
  int bpmin = var(0).position();
  int bpmax = var(0).stop();
  for (int i=1; i<size(); i++)
    {
      if ( var(i).chromosome() != chr ) return -1;
      if ( var(i).position() < bpmin ) 
	bpmin = var(i).position();
      if ( var(i).stop() > bpmax ) 
	bpmax = var(i).stop();      
    }
  return bpmax-bpmin+1;
}
