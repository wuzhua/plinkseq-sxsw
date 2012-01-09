#include "ibd.h"
#include "pseq.h"
#include "func.h"

using namespace std;

extern GStore g;


void Pseq::IBD::load_wrapper( const std::string & segment_list , const std::string & ibddb )
{

  Helper::checkFileExists( segment_list );

  // Expected format for IBD segments, tab-delimited
  // no header, 
  // exome ID, GWAS ID, CHR, BP1, BP2

  // 00028296        PT-BPA2 1       1       1120590 4182860
  // 00028296        Sw1_PT-1S7Q     2       1       1120590 4558398
  // 00028296        Sw3_PT-8VRU     1       1       1120590 4342968

  // alternatively, 
  // ID1  ID2  CHR  POS1  POS2
  
  // which gets double entered in the DB (with phenotype as 'missing') 
 


  // Load data   
  IBDDBase ibd( ibddb ); 
  ibd.load( segment_list );
  
}


struct Aux_ibd_sharing { 
  Pseq::IBD::IBDDBase * ibddb;  
};
 
void f_ibd_sharing( Variant & v , void * p )
{

  Aux_ibd_sharing * aux = (Aux_ibd_sharing*)p;
  
  // Find all individuals who carry a rare variant (which will
  // typically be a doubleton) and output all IBD sharing information
  // on those pairs.
  
  std::vector<std::string> p1, p2;

  const int n = v.size();
  
  // which is the minor allele?
//   int c     = 0; // minor allele
//   int c_tot = 0; // total counts	  
  bool altmin = v.n_minor_allele( );      
  
  std::vector<int> carrier;

  for (int i=0; i<n; i++)
    {
      if ( v(i).null() ) continue;
      int ac1 = v(i).minor_allele_count( altmin );
      if ( ac1 == 0 ) continue;
      carrier.push_back( i );
    }
  
  if ( carrier.size() < 2 ) return;

  Region vregion( v.chromosome() , v.position() , v.stop() );

  int pcnt = 0, scnt = 0;
  
  for (int i=0; i<carrier.size()-1; i++)
    for (int j=i+1; j<carrier.size(); j++)
      {

	// get segments shared by this pair that span the region.
	std::set<Region> regions = aux->ibddb->shared_for_pair( v.ind( carrier[i] )->id() , v.ind( carrier[j] )->id() );
	
	// does a region overlap this position? 
	bool overlap = vregion.within( regions );
	
	// find size of largest overlapping segment. 
	Region olap(1,0,0);
	
	std::set<Region>::iterator ii = regions.begin();
	while ( ii != regions.end() )
	  {
	    if ( vregion.overlaps( *ii ) && ii->length() > olap.length() ) olap = *ii;	  
	    ++ii;
	  }
	
	plog << "_P" << "\t" 
	     << v << "\t"	     
	     << carrier.size() << "\t"
	     << regions.size() << "\t"
	     << v.ind( carrier[i] )->id() << "\t"
	     << v.ind( carrier[j] )->id() << "\t"
	     << ( v.ind( carrier[i] )->affected() == CASE ) << "\t"
	     << ( v.ind( carrier[j] )->affected() == CASE ) << "\t"	  
	     << overlap << "\t";
	if ( overlap ) 
	  plog << olap << "\n";
	else 
	  plog << "NA" << "\n";       
	
	++pcnt;
	if ( overlap ) ++scnt;
	
      }
  
  plog << "_S" << "\t"
       << v << "\t"
       << carrier.size() << "\t"
       << scnt << "\t"
       << pcnt << "\n";
  
} 
 
void Pseq::IBD::sharing_wrapper( const std::string & ibddb_filename , Mask & m )
{  
  Helper::checkFileExists( ibddb_filename );
  IBDDBase ibddb( ibddb_filename );
  Aux_ibd_sharing aux;
  aux.ibddb = &ibddb;
  g.vardb.iterate( f_ibd_sharing , &aux , m );  
}



Pseq::IBD::IBDDBase::IBDDBase( const std::string & db )
{
  
  // Assume this database will be used in two ways

  // 1) where proband is in VARDB but partner is not (i.e. for
  //    s-assoc, which is why we store partner phenotype information
  //    here also)

  // 2) also where both members of a pair are in VARDB.  Here we do not double enter, 
  //    and there is no distinction between 'proband' and 'partner'. Here the basic query
  //    will be just to return all regions that a pair share

  sql.open(db);
  
  sql.synchronous(false);

  sql.query(" CREATE TABLE IF NOT EXISTS segs("
	    "   proband_id   VARCHAR(20) NOT NULL , "
	    "   partner_id   VARCHAR(20) NOT NULL , "
	    "   partner_phe  INTEGER NOT NULL , "
	    "   chr          INTEGER NOT NULL , "
	    "   bp1          INTEGER NOT NULL  , "
	    "   bp2          INTEGER NOT NULL  ); " );
  
  sql.query( "CREATE INDEX IF NOT EXISTS sIndex1 ON segs(proband_id,chr);" );
  sql.query( "CREATE INDEX IF NOT EXISTS sIndex2 ON segs(proband_id,partner_id);" );

  stmt_insert = sql.prepare(" INSERT OR IGNORE INTO segs"
			    " (proband_id, partner_id,partner_phe,chr,bp1,bp2) "
			    " values( :proband_id, :partner_id, :partner_phe, :chr, :bp1, :bp2 ) ; " );
  
  stmt_fetch = sql.prepare(" SELECT partner_id,partner_phe,bp1,bp2 "
 			   " FROM segs WHERE proband_id == :proband_id AND chr == :chr; ");

  stmt_fetch_pair = sql.prepare(" SELECT chr,bp1,bp2 "
				" FROM segs WHERE proband_id == :proband_id AND partner_id == :partner_id; ");
  
}

Pseq::IBD::IBDDBase::~IBDDBase()
{
  sql.finalise( stmt_insert );
  sql.finalise( stmt_fetch );
  sql.close();
}


void Pseq::IBD::IBDDBase::load( const std::string & filename )
{
  
  Helper::checkFileExists( filename );
  
  InFile f( filename );
  
  // Expected format (tab-separated)

  // ID(proband)   ID(partner)  PHE(partner) CHR  BP1  BP2

  int cnt = 0;
  
  sql.begin();
  
  while ( ! f.eof() )
    {
      std::string l = f.readLine();      
      if ( l == "" ) continue;      
      std::vector<std::string> buffer = Helper::char_split( l , '\t' );

      if ( buffer.size() == 6 ) 
	{
	  
	  int chr = Helper::chrCode( buffer[3] );
	  if ( chr == 0 ) continue;
	  int bp1 = Helper::str2int( buffer[4] );
	  int bp2 = Helper::str2int( buffer[5] );
	  int phe = Helper::str2int( buffer[2] );
	  
	  sql.bind_text( stmt_insert , ":proband_id" , buffer[0] );
	  sql.bind_text( stmt_insert , ":partner_id" , buffer[1] );
	  sql.bind_int( stmt_insert , ":partner_phe" , phe );
	  
	  sql.bind_int( stmt_insert , ":chr" , chr );
	  sql.bind_int( stmt_insert , ":bp1" , bp1 );
	  sql.bind_int( stmt_insert , ":bp2" , bp2 );
	  
	  sql.step( stmt_insert );
	  sql.reset( stmt_insert );
	  ++cnt;
	}
      else if ( buffer.size() == 5 ) 
	{
	  
	  int chr = Helper::chrCode( buffer[2] );
	  if ( chr == 0 ) continue;
	  int bp1 = Helper::str2int( buffer[3] );
	  int bp2 = Helper::str2int( buffer[4] );
	  
	  sql.bind_text( stmt_insert , ":proband_id" , buffer[0] );
	  sql.bind_text( stmt_insert , ":partner_id" , buffer[1] );
	  sql.bind_int( stmt_insert , ":partner_phe" , 0 );
	  
	  sql.bind_int( stmt_insert , ":chr" , chr );
	  sql.bind_int( stmt_insert , ":bp1" , bp1 );
	  sql.bind_int( stmt_insert , ":bp2" , bp2 );
	  
	  sql.step( stmt_insert );
	  sql.reset( stmt_insert );

	  // double entry
	  sql.bind_text( stmt_insert , ":proband_id" , buffer[1] );
	  sql.bind_text( stmt_insert , ":partner_id" , buffer[0] );
	  sql.step( stmt_insert );
	  sql.reset( stmt_insert );
	  
	  ++cnt;

	}
    }

  sql.commit();
  
  f.close();
  
  plog << cnt << " segments loaded\n";
}


std::vector<Pseq::IBD::IBDPartner> Pseq::IBD::IBDDBase::fetch( const std::string & id , const Region & r)
{
  
  sql.bind_text( stmt_fetch , ":proband_id" , id );
  sql.bind_int( stmt_fetch , ":chr" , r.chromosome() );
  int chr = r.chromosome();

  std::vector<Pseq::IBD::IBDPartner> shared;
  while( sql.step( stmt_fetch ) )
    {      
      int bp1 = sql.get_int( stmt_fetch , 2 );
      int bp2 = sql.get_int( stmt_fetch , 3 );
      Region r2(chr,bp1,bp2);
      if ( r.overlaps(r2) ) 
	shared.push_back( Pseq::IBD::IBDPartner( sql.get_text( stmt_fetch , 0 ) , 
						 sql.get_int( stmt_fetch , 1 ) ) );
    } 
  sql.reset( stmt_fetch );  
  return shared;
}


std::vector<Pseq::IBD::IBDPartnerRegion> Pseq::IBD::IBDDBase::fetch_regions( const std::string & id , const Region & r )
{  

  sql.bind_text( stmt_fetch , ":proband_id" , id );
  sql.bind_int( stmt_fetch , ":chr" , r.chromosome() );
  int chr = r.chromosome();
  
//   stmt_fetch = sql.prepare(" SELECT partner_id,partner_phe,bp1,bp2 "
//  			   " FROM segs WHERE proband_id == :proband_id AND chr == :chr; ");


  std::vector<Pseq::IBD::IBDPartnerRegion> shared;
  while( sql.step( stmt_fetch ) )
    {      
      
      int bp1 = sql.get_int( stmt_fetch , 2 );
      int bp2 = sql.get_int( stmt_fetch , 3 );
      
      Region r2( chr, bp1, bp2 );
      
      if ( r.overlaps(r2) ) 
	{
	  std::string id1 = sql.get_text( stmt_fetch , 0 );
	  shared.push_back( Pseq::IBD::IBDPartnerRegion( id1 , r2 ) );
	}
    } 
  sql.reset( stmt_fetch );
  return shared;
}


std::set<Region> Pseq::IBD::IBDDBase::shared_for_pair( const std::string & id1 , const std::string & id2 )
{
  std::set<Region> r;
  sql.bind_text( stmt_fetch_pair , ":proband_id" , id1 );
  sql.bind_text( stmt_fetch_pair , ":partner_id" , id2 );
  while ( sql.step( stmt_fetch_pair ) )
    {
      int chr = sql.get_int( stmt_fetch_pair , 0 );
      int bp1 = sql.get_int( stmt_fetch_pair , 1 );
      int bp2 = sql.get_int( stmt_fetch_pair , 2 );
      Region reg( chr , bp1 , bp2 );
      r.insert( reg );
    }
  sql.reset( stmt_fetch_pair );
  return r;
}


int2 Pseq::IBD::IBDDBase::case_control_count( const std::string & id , const Region & r )
{
  std::vector<Pseq::IBD::IBDPartner> p = fetch( id , r );
  
  int2 counts;
  for ( int i = 0; i < p.size() ; i++ )
    if ( p[i].affected == 1 ) counts.p1++;
    else counts.p2++;
  return counts;
}



int2 Pseq::IBD::IBDDBase::case_control_count( const std::string & id , const Region & r , 
						       std::map<std::string,int> & imap, 
						       std::vector<int> & pmap,
						       std::vector<int> & permed )
{
  
  std::vector<Pseq::IBD::IBDPartner> p = fetch( id , r );
  
  // Additional: 
  //  imap    ID-string  -->  original slot
  //  pmap    phenotype  in original order
  //  permed  shuffle, 
  //  so get phenotype by 
  // pmap[ permed[ imap[id] ] ]
  
  int2 counts;
  for ( int i = 0; i < p.size() ; i++ )
    {
      int phenotype = pmap[ permed[ imap[ p[i].id ] ] ];
      if ( phenotype == 2 ) counts.p1++;
      else counts.p2++;
    }
  return counts;
}



void random_draw( vector<int> & a )
{
  
  // Generate a random permutation of 0 to n-1 where n is a.size(),
  // using Fisher-Yates shuffle, simultaneously initializing from
  // 0..n-1 and shuffling
  
  const int n = a.size( ) ;  
  for( int i = 0; i < n; i++ )
    a[ i ] = i;

  int tmp;
  for( int i = n; i > 1;  i-- )
    {
      int j = CRandom::rand(i);
      tmp = a[i-1];
      a[i-1] = a[j];
      a[j] = tmp;
    }
}



void Pseq::IBD::test_wrapper( const std::string & segment_list , 
			      const std::string & gwas_phenotypes , 
			      int nrep, 
			      Mask & m )
{
  
  Helper::checkFileExists( segment_list );
  Helper::checkFileExists( gwas_phenotypes );
  
  // Does database already exist?
  
  bool db_exists = Helper::fileExists( segment_list + ".db" );
  
  IBDDBase ibd( segment_list + ".db" );
  
  // Expected format for IBD segments, tab-delimited
  // no header, 
  // exome ID, GWAS ID, CHR, BP1, BP2

  // 00028296        PT-BPA2 1       1       1120590 4182860
  // 00028296        Sw1_PT-1S7Q     2       1       1120590 4558398
  // 00028296        Sw3_PT-8VRU     1       1       1120590 4342968


  // Load data if the db was just newly created

  if ( ! db_exists )
    ibd.load( segment_list );
 
  
  //
  // Phenotypic data, and separate permutation class for 
  // the extended GWAS panel
  //
    
  std::vector<Individual> gwas_panel;
  
  std::ifstream IN1;
  
  int cnt = 0;
  
  std::map<std::string,int> imap; 
  std::vector<int> pmap;
  
  // Format: tab-delim
  // GWAS ID, phenotype (2=case, 1=control) 

  IN1.open( gwas_phenotypes.c_str() , std::ios::in );
  while ( ! IN1.eof() )
    {
      std::string id;
      int phe;
      IN1 >> id >> phe;
      if ( id == "" ) 
	continue;
      Individual i(id);
      if ( phe == 2 ) i.affected(CASE);
      else i.affected(CONTROL);
      gwas_panel.push_back(i);
      imap[ id ] = cnt++;
      pmap.push_back( phe );
    }
  
  if ( gwas_panel.size() == 0 ) 
    plog.warn("did not read any valid individuals from GWAS panel");
  
 

  //
  // Helper class
  //

  // Look only at singletons and doubletons for now

  Pseq::IBD::Aux a;  
  a.g    = &g;
  a.ibd  = &ibd;
  a.pmap = &pmap;
  a.imap = &imap;  
  a.rseed = time(0);

  g.perm.initiate( nrep );
  
  plog << "SET" << "\t"
       << "ALIAS" << "\t"
       << "NVAR" << "\t"
       << "AA" << "\t"
       << "AU" << "\t"
       << "P_B" << "\t"
       << "P_S" << "\n";  
  
  //
  // Apply test to dataset
  //

  g.vardb.iterate( g_STEST_association , &a , m );
  
  Pseq::finished();
}



void g_STEST_association( VariantGroup & vars , void * p )
{


  //
  // Is this a suitable group?
  //
  
  if ( vars.n_individuals() < 2 ) return;  
  if ( vars.n_variants() < 2 ) return;  
  
  
  //
  // Get auxiliary daya
  //

  if ( ! p ) return;
  Pseq::IBD::Aux * data = (Pseq::IBD::Aux*)p;
  GStore * g = data->g;
  

  //
  // Set up permutations
  //

  const int R = 1 + g->perm.replicates();

  g->perm.seed( data->rseed );
  g->perm.reset();


  // Separate empirical p-value for S-test component

  int emp2 = 1 ;
  

  //
  // Create summary output first
  //


  std::map<int,int> mc;       // key=#minor alleles,value=#variants
  std::set<int> refmin;       // track alleles at which reference is minor 
  std::map<std::string,int> mc_a;  // count in affecteds


  //
  // Track observed min/max minor allele counts
  //

  std::vector<bool> altmin;
  
  for ( int v = 0 ; v < vars.size(); v++ )
    altmin.push_back( vars(v).n_minor_allele() );
  

  vector<int> permed( data->pmap->size() );
  for (int i=0; i< permed.size() ;i++)
    permed[i] = i;
   

  const int n = vars.n_individuals();

  int original_aa = 0 , original_au = 0;
  

  //
  // Start permutations
  //
  
  for (int p = 0; p < R ; p++ )
    {
   
      // Sharing-statistics

      int aa = 0;
      int au = 0;


      // Manually permute GWA  panel
      
      if ( p > 0 ) 
	random_draw( permed );
      
  
      //
      // Calculate association statistics
      //         

      // Case-unique burden is the statistic
      
      std::set<int> uniq;

      double statistic = 0;
      
      for (int v=0; v<vars.size(); v++ ) 
	{
	  int alta = 0 , altu = 0; 
	  for (int i = 0; i < n; i++)
	    {

	      int j = g->perm.pos( i );	  
	      affType aff = vars(v).ind( j )->affected();	      
	      if ( vars.geno(v,i).minor_allele( altmin[v] ) )
		if ( aff == CASE ) { uniq.insert(v); alta++; } else altu++;	      
	    }
	  
	  // case-unique burden
	  if ( altu == 0 ) statistic += alta;
	  
	} // next variant
      
      
      //
      // Calculate S-test for case-only variants
      //


      for (int i = 0; i < n; i++)
	{
	  string original_id = vars.ind( i )->id();
	  affType original_aff = vars.ind( i )->affected();	  
	  
	  set<int>::iterator v = uniq.begin();
	  while ( v != uniq.end() )
	    {
	      int chr = vars.var(*v).chromosome();
	      int bp = vars.var(*v).position();
	      
	      if ( vars.geno(*v,i).minor_allele( altmin[*v] ) )
		{
		  if ( original_aff == CASE ) 
		    {
		      
		      //
		      // Get c/c counts from shared-segment dataset, that overlap this position
		      //
		      int2 cc = data->ibd->case_control_count( original_id , 
							       Region( chr, bp, bp ) , 
							       *(data->imap), 
							       *(data->pmap), 
							       permed
							       );
		      
		      aa += cc.p1;
		      au += cc.p2;
		      
		    }
		}

	      ++v;
	    }
	}
      
      //
      // Store...
      //

      if ( p == 0 ) 
	{
	  original_aa = aa;
	  original_au = au;
	}
      

      //
      // Store/permute basic test
      //

      g->perm.score( statistic );
      
      //
      // Store/permute S-test
      //
      
      if ( p>0 && ( aa-au ) >= ( original_aa - original_au ) ) ++emp2;
      
      
      
    } // Next permutation
  
  
  //
  // Output and return for next gene
  //

  
  plog << vars.name() << "\t"
       << g->locdb.alias( vars.name() , false ) << "\t"
       << vars.size() << "\t"
       << original_aa << "\t"
       << original_au << "\t"
       << g->perm.pvalue(0) << "\t"
       << emp2/(double)R << "\n";  
  
  return;

}



//
// Mutation-screen implementation
//

void f_ibd_mutation_screen_accumulator(Variant & v, void *p)
{
  VariantGroup * d = (VariantGroup*)p; 
  d->force_add(v);
}



void Pseq::IBD::mutation_wrapper( const std::string & ibddb_filename , 
				  const std::string & proband_id_str , 
				  const std::string & region_str , 
				  Mask & m )
{  
  
  Helper::checkFileExists( ibddb_filename );
  IBDDBase ibddb( ibddb_filename );
  
  bool okay = true;
  Region region( region_str , okay );
  if ( ! okay ) Helper::halt( "malformed region specification" );

  int id = g.indmap.ind_n( proband_id_str );
  if ( id == -1 ) Helper::halt( "could not find individual " + proband_id_str );


  //
  // 1) Lookup all segments of people in IBDDB who share with this proband at this position
  //

  std::vector<Pseq::IBD::IBDPartnerRegion> olap = ibddb.fetch_regions( proband_id_str , region );
  

  //
  // 2) Remove any people who do not exist in the VARDB (given a Mask even)
  // 3) Figure out the maximum shared region here, of the remaining pairs.
  //

  std::vector<bool> incl( olap.size() , true );

  std::cout << olap.size() << " is olap size\n";

  int lower_bound = region.start.position();
  int upper_bound = region.stop.position();
  int chr = region.chromosome();

  std::map<std::string,Region> xolap;

  for (int i = 0 ; i < incl.size(); i++ ) 
    {
      
      if ( g.indmap.ind_n( olap[i].id ) == -1 ) incl[i] = false;
      else if ( olap[i].id == proband_id_str ) incl[i] = false;      
      
      // should not happen, but check anyway
      if ( olap[i].region.chromosome() != chr ) incl[i] = false; 

      
      if ( incl[i] ) 
	{
	  if ( olap[i].region.start.position() < lower_bound ) 
	    lower_bound = olap[i].region.start.position() ;
	  if ( olap[i].region.stop.position() > upper_bound ) 
	    upper_bound = olap[i].region.stop.position();
	}


      // implies that another individual should only have a single
      // olaping segment here with any other individual, which should 
      // in general be okay

      xolap[ olap[i].id ] = olap[i].region ;

      
      // also add to mask -- NOTE -- changing the mask
      // like this will cause problems if we then
      // want to repeatedly call this -- will have to (partially)
      // reset the mask to the user's original string

      m.include_indiv( olap[i].id );

    }


  Region total( chr , lower_bound , upper_bound );

  plog << "found " << xolap.size() << " matches spanning " << total << "\n";

  // Add this as a 'reg' Mask
  //  -- note -- we hope / expect that no other 'reg' mask has been given... 
  //             add a check at some point

  m.include_reg( total );
  

  VariantGroup vars(m);

  
  //
  // 4) Pull the regions variants into vars
  //
  
  g.vardb.iterate( f_ibd_mutation_screen_accumulator , &vars , m ); 
  

  //
  // 5) Get the variant; now given the region
  //

  int nv = vars.size();
  int nind = g.indmap.size();
  
  // recast 'id'
  id = g.indmap.ind_n( proband_id_str );

  // track for proband:
  std::vector<int>  n_invalid(nv,0);     // # of inconsistent matches
  std::vector<int>  n_ibd(nv,0);         // total number of individuals who apparent are IBD at this point
  std::vector<bool> is_het(nv,false);    // indicator if proband if heterozygote here
  std::vector<int>  n_support_a1(nv,0);  // number of matches for a1 for proband hets
  std::vector<int>  n_support_a2(nv,0);  // number of matches for a2 for proband hets
  
  std::cout << "\n\n" << nv << " is nv\n\n";

  for (int v = 0 ; v < nv ; v++ ) 
    {
      plog << vars(v) << "\t" << vars(v).label( id ) << "\n";
      if ( vars(v,id).heterozygote() ) is_het[v] = true;
    }


  //
  // For each paired individual, figure out the shared allele, if possible
  //
  
  std::map<std::string,Region>::iterator xi = xolap.begin();

  while ( xi != xolap.end() )
    {
      
      int i = g.indmap.ind_n( xi->first );
      
      std::cout << " xi->first = " << xi->first << " " << i << "\n";
      
      Region & reg = xi->second;

      if ( i == id ) { ++xi; continue; } // do not count self
      
      plog << vars.ind( i )->id() << " shares " << reg << "\n";
      
      for (int v = 0 ; v < nv ; v++ ) 
	{

	  Variant & var = vars(v);
	  if ( ! reg.contains( var ) ) { ++xi; continue; }
	  
	  // track overall number of people who share at this position
	  n_ibd[ v ]++;
	  
	  Genotype & gproband = var( id );
	  Genotype & gpartner = var( i );
	  
	  if ( gproband.null() || gpartner.null() ) { ++xi; continue; }
	  if ( gproband.heterozygote() && gpartner.heterozygote() ) { ++xi; continue; }
	  
	  // NOTE: assumes a biallelic SNP 
	  
	  int acproband = gproband.allele_count();
	  int acpartner = gpartner.allele_count();
	  
	  // inconsistent match?
	  
	  if ( ( acproband == 0 && acpartner == 2 ) || 
	       ( acproband == 2 && acpartner == 0 ) ) 
	    {
	      n_invalid[v]++;
	      ++xi;
	      continue;
	    }
	  
	  // is this a proband het site?
	  
	  if ( is_het[v] )
	    {
	      if ( acpartner == 1 ) { ++xi; continue; } // ambiguous double het scenario
	      if ( acpartner == 0 ) n_support_a1[v]++;
	      if ( acpartner == 2 ) n_support_a2[v]++;
	    }
	  
	}

      // next partner
      ++xi;
    }


  //  Proband    IBD1                Summary

  //
  //    A/A      A/A  -> A    
  //    A/C  *   A/A  -> A           A/C/?/x
  //    C/C      A/C  -> C           
  //    A/C  *   A/C  -> *
  //    M/m      m/m  -> n/a
  //    A/A      A/C  -> A
  //    C/C      A/C  -> C
  //    A/C  *   C/C  -> C   
  //    A/A      
  //


  // Notes.  Ultimately we are looking to see conclusive evidence of
  // both alleles of the proband in others. This kind of definition
  // should be quite robust to errors, misplaced margins, etc, as we
  // are not trying to match whole haplotypes, but rather just 
  // determine support for having seen
  
  // We now need to make some kind of decision -- do we see support for
  
  // We are assuming that the singleton (i.e. exome) calls wasn't used
  // in the IBD estimation (i.e. from GWAS) -- although probably would
  // not matter much (althogh if v. rare could decrease P(IBD) for
  // that pair.

  // We could also check for a higher level of consistency when >1
  // individual shares -- i.e they should share the same haplotype
  // across all, or do the data suggest >2 haplotypes are present in 
  // the proband.

  // We assume that both the GWAS and the exome data will be in
  // VARDB. We need the GWAS data to have enough hets from common SNPs
  // to resolve haplotype identity.
  
  // The a1 and a2 counts do not need to be equal (i.e. it could just
  // happen that one haplotype is more common in the pop -- we just need to 
  // see that both are >0.

  
  for (int v = 0 ; v < nv ; v++ ) 
    {
      plog << vars(v) << "\t" 
	   << vars(v).label( id ) << "\t"
	   << n_ibd[v] << "\t"
	   << is_het[v] << "\t"
	   << n_support_a1[v] << "\t"
	   << n_support_a2[v] << "\n";

    }

  
  
}

