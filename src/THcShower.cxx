///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// THcShower                                                                 //
//                                                                           //
// Shower counter class, describing a generic segmented shower detector.     //
//                                                                           //
//                                                                           //
//                                                                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "THcShower.h"
#include "THcShowerCluster.h"
#include "THaEvData.h"
#include "THaDetMap.h"
#include "THcDetectorMap.h"
#include "THcGlobals.h"
#include "THaCutList.h"
#include "THcParmList.h"
#include "VarDef.h"
#include "VarType.h"
#include "THaTrack.h"
#include "TClonesArray.h"
#include "TMath.h"

#include "THaTrackProj.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using namespace std;

//_____________________________________________________________________________
THcShower::THcShower( const char* name, const char* description,
				  THaApparatus* apparatus ) :
  THaNonTrackingDetector(name,description,apparatus)
{
  // Constructor
//  fTrackProj = new TClonesArray( "THaTrackProj", 5 );
  fNLayers = 0;			// No layers until we make them
}

//_____________________________________________________________________________
THcShower::THcShower( ) :
  THaNonTrackingDetector()
{
  // Constructor
}

void THcShower::Setup(const char* name, const char* description)
{

  char prefix[2];

  prefix[0] = tolower(GetApparatus()->GetName()[0]);
  prefix[1] = '\0';

  string layernamelist;
  DBRequest list[]={
    {"cal_num_layers", &fNLayers, kInt},
    {"cal_layer_names", &layernamelist, kString},
    {0}
  };

  gHcParms->LoadParmValues((DBRequest*)&list,prefix);
  cout << layernamelist << endl;
  cout << "Shower Counter: " << fNLayers << " layers" << endl;

  vector<string> layer_names = vsplit(layernamelist);

  if(layer_names.size() != (UInt_t) fNLayers) {
    cout << "ERROR: Number of layers " << fNLayers << " doesn't agree with number of layer names " << layer_names.size() << endl;
    // Should quit.  Is there an official way to quit?
  }

  fLayerNames = new char* [fNLayers];
  for(Int_t i=0;i<fNLayers;i++) {
    fLayerNames[i] = new char[layer_names[i].length()];
    strcpy(fLayerNames[i], layer_names[i].c_str());
  }
  
  char *desc = new char[strlen(description)+100];
  fPlanes = new THcShowerPlane* [fNLayers];

  for(Int_t i=0;i < fNLayers;i++) {
    strcpy(desc, description);
    strcat(desc, " Plane ");
    strcat(desc, fLayerNames[i]);

    fPlanes[i] = new THcShowerPlane(fLayerNames[i], desc, i+1, this); 
    cout << "Created Shower Plane " << fLayerNames[i] << ", " << desc << endl;
  }
}


//_____________________________________________________________________________
THaAnalysisObject::EStatus THcShower::Init( const TDatime& date )
{
  static const char* const here = "Init()";

  cout << "THcShower::Init " << GetName() << endl;
  Setup(GetName(), GetTitle());

  // Should probably put this in ReadDatabase as we will know the
  // maximum number of hits after setting up the detector map

  THcHitList::InitHitList(fDetMap, "THcRawShowerHit", 100);

  EStatus status;
  if( (status = THaNonTrackingDetector::Init( date )) )
    return fStatus=status;

  for(Int_t ip=0;ip<fNLayers;ip++) {
    if((status = fPlanes[ip]->Init( date ))) {
      return fStatus=status;
    }
  }

  char EngineDID[] = " CAL";
  EngineDID[0] = toupper(GetApparatus()->GetName()[0]);

  if( gHcDetectorMap->FillMap(fDetMap, EngineDID) < 0 ) {
    Error( Here(here), "Error filling detectormap for %s.", 
	     EngineDID);
      return kInitError;
  }

  return fStatus = kOK;
}

//_____________________________________________________________________________
Int_t THcShower::ReadDatabase( const TDatime& date )
{
  // Read this detector's parameters from the database file 'fi'.
  // This function is called by THaDetectorBase::Init() once at the
  // beginning of the analysis.
  // 'date' contains the date/time of the run being analyzed.

  //  static const char* const here = "ReadDatabase()";
  char prefix[2];

  // Read data from database 
  // Pull values from the THcParmList instead of reading a database
  // file like Hall A does.

  // We will probably want to add some kind of method to gHcParms to allow
  // bulk retrieval of parameters of interest.

  // Will need to determine which spectrometer in order to construct
  // the parameter names (e.g. hscin_1x_nr vs. sscin_1x_nr)

  cout << "THcShower::ReadDatabase called " << GetName() << endl;

  prefix[0]=tolower(GetApparatus()->GetName()[0]);
  prefix[1]='\0';

  {
    DBRequest list[]={
      {"cal_num_neg_columns", &fNegCols, kInt},
      {"cal_slop", &fSlop, kDouble},
      {"cal_fv_test", &fvTest, kDouble},
      {0}
    };
    gHcParms->LoadParmValues((DBRequest*)&list, prefix);
  }

  cout << "Number of neg. columns   = " << fNegCols << endl;
  cout << "Slop parameter           = " << fSlop << endl;
  cout << "Fiducial volum test flag = " << fvTest << endl;

  BlockThick = new Double_t [fNLayers];
  fNBlocks = new Int_t [fNLayers];
  fNLayerZPos = new Double_t [fNLayers];
  XPos = new Double_t [2*fNLayers];

  for(Int_t i=0;i<fNLayers;i++) {
    DBRequest list[]={
      {Form("cal_%s_thick",fLayerNames[i]), &BlockThick[i], kDouble},
      {Form("cal_%s_nr",fLayerNames[i]), &fNBlocks[i], kInt},
      {Form("cal_%s_zpos",fLayerNames[i]), &fNLayerZPos[i], kDouble},
      {Form("cal_%s_left",fLayerNames[i]), &XPos[2*i], kDouble},
      {Form("cal_%s_right",fLayerNames[i]), &XPos[2*i+1], kDouble},
      {0}
    };
    gHcParms->LoadParmValues((DBRequest*)&list, prefix);
  }

  //Caution! Z positions (fronts) are off in hcal.param! Correct later on.

  YPos = new Double_t* [fNLayers];
  for(Int_t i=0;i<fNLayers;i++) {
    YPos[i] = new Double_t [fNBlocks[i]];
    DBRequest list[]={
      {Form("cal_%s_top",fLayerNames[i]),YPos[i], kDouble, fNBlocks[i]},
      {0}
    };
    gHcParms->LoadParmValues((DBRequest*)&list, prefix);
  }

  for(Int_t i=0;i<fNLayers;i++) {
    cout << "Plane " << fLayerNames[i] << ":" << endl;
    cout << "    Block thickness: " << BlockThick[i] << endl;
    cout << "    NBlocks        : " << fNBlocks[i] << endl;
    cout << "    Z Position     : " << fNLayerZPos[i] << endl;
    cout << "    X Positions    : " << XPos[2*i] << ", " << XPos[2*i+1] << endl;
    cout << "    Y Positions    :";
    for(Int_t j=0; j<fNBlocks[i]; j++) {
      cout << " " << YPos[i][j];
    }
    cout << endl;

  }

  //Calibration related parameters (from hcal.param).

  fNtotBlocks=0;              //total number of blocks
  for (Int_t i=0; i<fNLayers; i++) fNtotBlocks += fNBlocks[i];

  cout << "Total number of blocks in the calorimeter: " << fNtotBlocks << endl;

  //Pedestal limits from hcal.param.
  fShPosPedLimit = new Int_t [fNtotBlocks];
  fShNegPedLimit = new Int_t [fNtotBlocks];

  //Calibration constants
  fPosGain = new Double_t [fNtotBlocks];
  fNegGain = new Double_t [fNtotBlocks];

  //Read in parameters from hcal.param
  Double_t hcal_pos_cal_const[fNtotBlocks];
  //  Double_t hcal_pos_gain_ini[fNtotBlocks];
  //  Double_t hcal_pos_gain_cur[fNtotBlocks];
  //  Int_t    hcal_pos_ped_limit[fNtotBlocks];
  Double_t hcal_pos_gain_cor[fNtotBlocks];

  Double_t hcal_neg_cal_const[fNtotBlocks];
  //  Double_t hcal_neg_gain_ini[fNtotBlocks];
  //  Double_t hcal_neg_gain_cur[fNtotBlocks];
  //  Int_t    hcal_neg_ped_limit[fNtotBlocks];
  Double_t hcal_neg_gain_cor[fNtotBlocks];

  DBRequest list[]={
    {"cal_pos_cal_const", hcal_pos_cal_const, kDouble, fNtotBlocks},
    //    {"cal_pos_gain_ini",  hcal_pos_gain_ini,  kDouble, fNtotBlocks},
    //    {"cal_pos_gain_cur",  hcal_pos_gain_cur,  kDouble, fNtotBlocks},
    {"cal_pos_ped_limit", fShPosPedLimit, kInt,    fNtotBlocks},
    {"cal_pos_gain_cor",  hcal_pos_gain_cor,  kDouble, fNtotBlocks},
    {"cal_neg_cal_const", hcal_neg_cal_const, kDouble, fNtotBlocks},
    //    {"cal_neg_gain_ini",  hcal_neg_gain_ini,  kDouble, fNtotBlocks},
    //    {"cal_neg_gain_cur",  hcal_neg_gain_cur,  kDouble, fNtotBlocks},
    {"cal_neg_ped_limit", fShNegPedLimit, kInt,    fNtotBlocks},
    {"cal_neg_gain_cor",  hcal_neg_gain_cor,  kDouble, fNtotBlocks},
    {"cal_min_peds", &fShMinPeds, kInt},
    {0}
  };
  gHcParms->LoadParmValues((DBRequest*)&list, prefix);

  //+++

  cout << "hcal_pos_cal_const:" << endl;
  for (Int_t j=0; j<fNLayers; j++) {
    for (Int_t i=0; i<fNBlocks[j]; i++) {
      cout << hcal_pos_cal_const[j*fNBlocks[j]+i] << " ";
    };
    cout <<  endl;
  };

  //  cout << "hcal_pos_gain_ini:" << endl;
  //  for (Int_t j=0; j<fNLayers; j++) {
  //    for (Int_t i=0; i<fNBlocks[j]; i++) {
  //      cout << hcal_pos_gain_ini[j*fNBlocks[j]+i] << " ";
  //    };
  //    cout <<  endl;
  //  };

  //  cout << "hcal_pos_gain_cur:" << endl;
  //  for (Int_t j=0; j<fNLayers; j++) {
  //    for (Int_t i=0; i<fNBlocks[j]; i++) {
  //      cout << hcal_pos_gain_cur[j*fNBlocks[j]+i] << " ";
  //    };
  //    cout <<  endl;
  //  };

  cout << "fShPosPedLimit:" << endl;
  for (Int_t j=0; j<fNLayers; j++) {
    for (Int_t i=0; i<fNBlocks[j]; i++) {
      cout << fShPosPedLimit[j*fNBlocks[j]+i] << " ";
    };
    cout <<  endl;
  };

  cout << "hcal_pos_gain_cor:" << endl;
  for (Int_t j=0; j<fNLayers; j++) {
    for (Int_t i=0; i<fNBlocks[j]; i++) {
      cout << hcal_pos_gain_cor[j*fNBlocks[j]+i] << " ";
    };
    cout <<  endl;
  };

  //---

  cout << "hcal_neg_cal_const:" << endl;
  for (Int_t j=0; j<fNLayers; j++) {
    for (Int_t i=0; i<fNBlocks[j]; i++) {
      cout << hcal_neg_cal_const[j*fNBlocks[j]+i] << " ";
    };
    cout <<  endl;
  };

  //  cout << "hcal_neg_gain_ini:" << endl;
  //  for (Int_t j=0; j<fNLayers; j++) {
  //    for (Int_t i=0; i<fNBlocks[j]; i++) {
  //      cout << hcal_neg_gain_ini[j*fNBlocks[j]+i] << " ";
  //    };
  //  //    cout <<  endl;
  //  };

  //  cout << "hcal_neg_gain_cur:" << endl;
  //  for (Int_t j=0; j<fNLayers; j++) {
  //    for (Int_t i=0; i<fNBlocks[j]; i++) {
  //      cout << hcal_neg_gain_cur[j*fNBlocks[j]+i] << " ";
  //    };
  //    cout <<  endl;
  //  };

  cout << "fShNegPedLimit:" << endl;
  for (Int_t j=0; j<fNLayers; j++) {
    for (Int_t i=0; i<fNBlocks[j]; i++) {
      cout << fShNegPedLimit[j*fNBlocks[j]+i] << " ";
    };
    cout <<  endl;
  };

  cout << "hcal_neg_gain_cor:" << endl;
  for (Int_t j=0; j<fNLayers; j++) {
    for (Int_t i=0; i<fNBlocks[j]; i++) {
      cout << hcal_neg_gain_cor[j*fNBlocks[j]+i] << " ";
    };
    cout <<  endl;
  };

  //Calibration constants in GeV per ADC channel.

  for (Int_t i=0; i<fNtotBlocks; i++) {
    fPosGain[i] = hcal_pos_cal_const[i] *  hcal_pos_gain_cor[i];
    fNegGain[i] = hcal_neg_cal_const[i] *  hcal_neg_gain_cor[i];
  }

  cout << "fPosGain:" << endl;
  for (Int_t j=0; j<fNLayers; j++) {
    for (Int_t i=0; i<fNBlocks[j]; i++) {
      cout << fPosGain[j*fNBlocks[j]+i] << " ";
    };
    cout <<  endl;
  };

  cout << "fNegGain:" << endl;
  for (Int_t j=0; j<fNLayers; j++) {
    for (Int_t i=0; i<fNBlocks[j]; i++) {
      cout << fNegGain[j*fNBlocks[j]+i] << " ";
    };
    cout <<  endl;
  };

  fIsInit = true;

  return kOK;
}



//_____________________________________________________________________________
Int_t THcShower::DefineVariables( EMode mode )
{
  // Initialize global variables and lookup table for decoder

  if( mode == kDefine && fIsSetup ) return kOK;
  fIsSetup = ( mode == kDefine );

  cout << "THcShower::DefineVariables called " << GetName() << endl;

  // Register variables in global list

 //  RVarDef vars[] = {
 //   { "nhit",   "Number of hits",                     "fNhits" },
 //   { "a",      "Raw ADC amplitudes",                 "fA" },
 //   { "a_p",    "Ped-subtracted ADC amplitudes",      "fA_p" },
 //   { "a_c",    "Calibrated ADC amplitudes",          "fA_c" },
 //   { "asum_p", "Sum of ped-subtracted ADCs",         "fAsum_p" },
 //   { "asum_c", "Sum of calibrated ADCs",             "fAsum_c" },
 //   { "nclust", "Number of clusters",                 "fNclust" },
 //   { "e",      "Energy (MeV) of largest cluster",    "fE" },
 //   { "x",      "x-position (cm) of largest cluster", "fX" },
 //   { "y",      "y-position (cm) of largest cluster", "fY" },
 //   { "mult",   "Multiplicity of largest cluster",    "fMult" },
 //   { "nblk",   "Numbers of blocks in main cluster",  "fNblk" },
 //   { "eblk",   "Energies of blocks in main cluster", "fEblk" },
 //   { "trx",    "track x-position in det plane",      "fTRX" },
 //   { "try",    "track y-position in det plane",      "fTRY" },
 //   { 0 }
 // };
 //  return DefineVarsFromList( vars, mode );
  return kOK;
}

//_____________________________________________________________________________
THcShower::~THcShower()
{
  // Destructor. Remove variables from global list.

  if( fIsSetup )
    RemoveVariables();
  if( fIsInit )
    DeleteArrays();
  if (fTrackProj) {
    fTrackProj->Clear();
    delete fTrackProj; fTrackProj = 0;
  }
}

//_____________________________________________________________________________
void THcShower::DeleteArrays()
{
  // Delete member arrays. Used by destructor.

  delete [] BlockThick;  BlockThick = NULL;
  delete [] fNBlocks;  fNBlocks = NULL;
  delete [] fNLayerZPos;  fNLayerZPos = NULL;
  delete [] XPos;  XPos = NULL;
  delete [] YPos;  YPos = NULL;
  //delete [] fSpacing;  fSpacing = NULL;
  //delete [] fCenter;   fCenter = NULL; // This 2D. What is correct way to delete?
}

//_____________________________________________________________________________
inline 
void THcShower::Clear(Option_t* opt)
{
//   Reset per-event data.
  for(Int_t ip=0;ip<fNLayers;ip++) {
    fPlanes[ip]->Clear(opt);
  }
 // fTrackProj->Clear();
}

//_____________________________________________________________________________
Int_t THcShower::Decode( const THaEvData& evdata )
{
  // Get the Hall C style hitlist (fRawHitList) for this event
  Int_t nhits = THcHitList::DecodeToHitList(evdata);

  if(gHaCuts->Result("Pedestal_event")) {
    Int_t nexthit = 0;
    for(Int_t ip=0;ip<fNLayers;ip++) {
      nexthit = fPlanes[ip]->AccumulatePedestals(fRawHitList, nexthit);
      //cout << "nexthit = " << nexthit << endl;
    }
    fAnalyzePedestals = 1;	// Analyze pedestals first normal events
    return(0);
  }

  if(fAnalyzePedestals) {
    for(Int_t ip=0;ip<fNLayers;ip++) {
      fPlanes[ip]->CalculatePedestals();
    }
    fAnalyzePedestals = 0;	// Don't analyze pedestals next event
  }

  Int_t nexthit = 0;
  for(Int_t ip=0;ip<fNLayers;ip++) {
    nexthit = fPlanes[ip]->ProcessHits(fRawHitList, nexthit);
  }

  //   fRawHitList is TClones array of THcRawShowerHit objects
  //  cout << "THcShower::Decode: Shower raw hit list:" << endl;
  //  for(Int_t ihit = 0; ihit < fNRawHits ; ihit++) {
  //    THcRawShowerHit* hit = (THcRawShowerHit *) fRawHitList->At(ihit);
  //    cout << ihit << " : " << hit->fPlane << ":" << hit->fCounter << " : "
  //	 << hit->fADC_pos << " " << hit->fADC_neg << " "  << endl;
  //  }
  //  cout << endl;

  return nhits;
}

//_____________________________________________________________________________
Int_t THcShower::ApplyCorrections( void )
{
  return(0);
}

//_____________________________________________________________________________
//Double_t THcShower::TimeWalkCorrection(const Int_t& paddle,
//					     const ESide side)
//{
//  return(0.0);
//}

//_____________________________________________________________________________
Int_t THcShower::CoarseProcess( TClonesArray&  ) //tracks
{
  // Calculation of coordinates of particle track cross point with shower
  // plane in the detector coordinate system. For this, parameters of track 
  // reconstructed in THaVDC::CoarseTrack() are used.
  //
  // Apply corrections and reconstruct the complete hits.
  //
  //  static const Double_t sqrt2 = TMath::Sqrt(2.);
  
  cout << "THcShower::CoarseProcess called ---------------------------" <<endl;

  //  ApplyCorrections();

  //
  // Clustering of hits.
  //

  THcShowerHitList HitList;                    //list of unclusterd hits

  for(Int_t j=0; j < fNLayers; j++) {

   //cout << "Plane " << j << "  Eplane = " << fPlanes[j]->GetEplane() << endl;

    for (Int_t i=0; i<fNBlocks[j]; i++) {

      Float_t Edep = fPlanes[j]->GetEmean(i);
      if (Edep > 0.) {                                    //hit
	Float_t y = YPos[j][i] + BlockThick[j]/2.;        //top + thick/2
	Float_t z = fNLayerZPos[j] + BlockThick[j]/2.;    //front + thick/2
	THcShowerHit* hit = new THcShowerHit(i,j,y,z,Edep);

	HitList.push_back(hit);

	//cout << "Hit: Edep = " << Edep << " Y = " << y << " Z = " << z <<
	//	  " Block " << i << " Layer " << j << endl;
      };

    }
  }

  //Print out hits before clustering.
  //
  cout << "Total hits:     " << HitList.size() << endl;
  for (unsigned int i=0; i!=HitList.size(); i++) {
    cout << "unclustered hit " << i << ": ";
    (*(HitList.begin()+i))->show();
  }

  THcShowerClusterList* ClusterList = new THcShowerClusterList;
  ClusterList->ClusterHits(HitList);

  //Print out the cluster list.
  //
  cout << "Cluster_list size: " << (*ClusterList).NbClusters() << endl;

  for (unsigned int i=0; i!=(*ClusterList).NbClusters(); i++) {

    THcShowerCluster* cluster = (*ClusterList).ListedCluster(i);

    cout << "Cluster #" << i 
         <<":  E=" << (*cluster).clE() 
         << "  Epr=" << (*cluster).clEpr()
         << "  Y=" << (*cluster).clY()
         << "  Z=" << (*cluster).clZ()
         << "  size=" << (*cluster).clSize()
         << endl;

    for (unsigned int j=0; j!=(*cluster).clSize(); j++) {
      THcShowerHit* hit = (*cluster).ClusteredHit(j);
      cout << "  hit #" << j << ":  "; (*hit).show();
    }

  }
   
  cout << "THcShower::CoarseProcess return ---------------------------" <<endl;
  return 0;
}

//_____________________________________________________________________________
Int_t THcShower::FineProcess( TClonesArray& tracks )
{
  // Reconstruct coordinates of particle track cross point with shower
  // plane, and copy the data into the following local data structure:
  //
  // Units of measurements are meters.

  // Calculation of coordinates of particle track cross point with shower
  // plane in the detector coordinate system. For this, parameters of track 
  // reconstructed in THaVDC::FineTrack() are used.

  return 0;
}

ClassImp(THcShower)
////////////////////////////////////////////////////////////////////////////////
 
