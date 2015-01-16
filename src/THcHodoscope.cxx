///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// THcHodoscope                                                              //
//                                                                           //
// Class for a generic hodoscope consisting of multiple                      //
// planes with multiple paddles with phototubes on both ends.                //
// This differs from Hall A scintillator class in that it is the whole       //
// hodoscope array, not just one plane.                                      //
//                                                                           //
// Date July 8 2014:                                                         //
// Zafr Ahmed                                                                //
// Beta and chis square are calculated for each of the hodoscope track.      //
// Two new variables are added. fBeta and fBetaChisq                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "THcSignalHit.h"
#include "THcShower.h"

#include "THcHitList.h"
#include "THcRawShowerHit.h"
#include "TClass.h"
#include "math.h"
#include "THaSubDetector.h"

#include "THcHodoscope.h"
#include "THaEvData.h"
#include "THaDetMap.h"
#include "THcDetectorMap.h"
#include "THaGlobals.h"
#include "THaCutList.h"
#include "THcGlobals.h"
#include "THcParmList.h"
#include "VarDef.h"
#include "VarType.h"
#include "THaTrack.h"
#include "TClonesArray.h"
#include "TMath.h"

#include "THaTrackProj.h"
#include <vector>

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>

using namespace std;
using std::vector;

//_____________________________________________________________________________
THcHodoscope::THcHodoscope( const char* name, const char* description,
				  THaApparatus* apparatus ) :
  THaNonTrackingDetector(name,description,apparatus)
{
  // Constructor

  //fTrackProj = new TClonesArray( "THaTrackProj", 5 );
  // Construct the planes
  fNPlanes = 0;			// No planes until we make them
  fStartTime=-1e5;
  fGoodStartTime=kFALSE;
}

//_____________________________________________________________________________
THcHodoscope::THcHodoscope( ) :
  THaNonTrackingDetector()
{
  // Constructor
}

//_____________________________________________________________________________
void THcHodoscope::Setup(const char* name, const char* description)
{

  //  static const char* const here = "Setup()";
  //  static const char* const message = 
  //    "Must construct %s detector with valid name! Object construction failed.";

  cout << "In THcHodoscope::Setup()" << endl;
  // Base class constructor failed?
  if( IsZombie()) return;

  fDebug   = 1;  // Keep this at one while we're working on the code    

  char prefix[2];

  prefix[0]=tolower(GetApparatus()->GetName()[0]);
  prefix[1]='\0';

  string planenamelist;
  DBRequest listextra[]={
    {"hodo_num_planes", &fNPlanes, kInt},
    {"hodo_plane_names",&planenamelist, kString},
    {0}
  };
  //fNPlanes = 4; 		// Default if not defined
  gHcParms->LoadParmValues((DBRequest*)&listextra,prefix);
  
  cout << "Plane Name List : " << planenamelist << endl;

  vector<string> plane_names = vsplit(planenamelist);
  // Plane names  
  if(plane_names.size() != (UInt_t) fNPlanes) {
    cout << "ERROR: Number of planes " << fNPlanes << " doesn't agree with number of plane names " << plane_names.size() << endl;
    // Should quit.  Is there an official way to quit?
  }
  fPlaneNames = new char* [fNPlanes];
  for(UInt_t i=0;i<fNPlanes;i++) {
    fPlaneNames[i] = new char[plane_names[i].length()+1];
    strcpy(fPlaneNames[i], plane_names[i].c_str());
  }

  //myShower = new THcShower("cal", "Shower" );

  /*  fPlaneNames = new char* [fNPlanes];
  for(Int_t i=0;i<fNPlanes;i++) {fPlaneNames[i] = new char[3];}
  // Should get the plane names from parameters.  
  // could try this: grep _zpos PARAM/hhodo.pos | sed 's/\_/\ /g' | awk '{print $2}'
  strcpy(fPlaneNames[0],"1x");  
  strcpy(fPlaneNames[1],"1y");
  strcpy(fPlaneNames[2],"2x");
  strcpy(fPlaneNames[3],"2y");
  */
  // Probably shouldn't assume that description is defined
  char* desc = new char[strlen(description)+100];
  fPlanes = new THcScintillatorPlane* [fNPlanes];
  for(UInt_t i=0;i < fNPlanes;i++) {
    strcpy(desc, description);
    strcat(desc, " Plane ");
    strcat(desc, fPlaneNames[i]);
    fPlanes[i] = new THcScintillatorPlane(fPlaneNames[i], desc, i+1,fNPlanes,this); // Number planes starting from zero!!
    cout << "Created Scintillator Plane " << fPlaneNames[i] << ", " << desc << endl;
  }
  delete [] desc;
}

//_____________________________________________________________________________
THaAnalysisObject::EStatus THcHodoscope::Init( const TDatime& date )
{
  cout << "In THcHodoscope::Init()" << endl;
  Setup(GetName(), GetTitle());

  // Should probably put this in ReadDatabase as we will know the
  // maximum number of hits after setting up the detector map
  // But it needs to happen before the sub detectors are initialized
  // so that they can get the pointer to the hitlist.

  // --------------- To get energy from THcShower ----------------------

  const char* shower_detector_name = "cal";  
  THaApparatus* app = GetApparatus();
  THaDetector* det = app->GetDetector( shower_detector_name );

  if( !dynamic_cast<THcShower*>(det) ) {
    Error("THcHodoscope", "Cannot find shower detector %s",
   	  shower_detector_name );
    return fStatus = kInitError;
  }

  fShower = static_cast<THcShower*>(det);     // fShower is a membervariable
  
  // --------------- To get energy from THcShower ----------------------


  InitHitList(fDetMap, "THcRawHodoHit", 100);

  EStatus status;
  // This triggers call of ReadDatabase and DefineVariables
  if( (status = THaNonTrackingDetector::Init( date )) )
    return fStatus=status;

  for(UInt_t ip=0;ip<fNPlanes;ip++) {
    if((status = fPlanes[ip]->Init( date ))) {
      return fStatus=status;
    }
  }

  // Replace with what we need for Hall C
  //  const DataDest tmp[NDEST] = {
  //    { &fRTNhit, &fRANhit, fRT, fRT_c, fRA, fRA_p, fRA_c, fROff, fRPed, fRGain },
  //    { &fLTNhit, &fLANhit, fLT, fLT_c, fLA, fLA_p, fLA_c, fLOff, fLPed, fLGain }
  //  };
  //  memcpy( fDataDest, tmp, NDEST*sizeof(DataDest) );

  char EngineDID[]="xSCIN";
  EngineDID[0] = toupper(GetApparatus()->GetName()[0]);
  if( gHcDetectorMap->FillMap(fDetMap, EngineDID) < 0 ) {
    static const char* const here = "Init()";
    Error( Here(here), "Error filling detectormap for %s.", 
	     EngineDID);
      return kInitError;
  }

  fNScinHits     = new Int_t [fNPlanes];
  fGoodPlaneTime = new Bool_t [fNPlanes];
  fNPlaneTime    = new Int_t [fNPlanes];
  fSumPlaneTime  = new Double_t [fNPlanes];

  //  Double_t  fHitCnt4 = 0., fHitCnt3 = 0.;
  
  fScinHit = new Double_t*[fNPlanes];         
  for (UInt_t m = 0; m < fNPlanes; m++ ){
    fScinHit[m] = new Double_t[fNPaddle[0]];
  }
  

  return fStatus = kOK;
}
//_____________________________________________________________________________
Double_t THcHodoscope::DefineDoubleVariable(const char* fName)
{
  // Define a variale of type double by looking it up in the THcParmList
  char prefix[2];
  char parname[100];
  Double_t tmpvar=-1e6;
  prefix[0]=tolower(GetApparatus()->GetName()[0]);
  prefix[1]='\0';
  strcpy(parname,prefix);
  strcat(parname,fName);
  if (gHcParms->Find(parname)) {
    tmpvar=*(Double_t *)gHcParms->Find(parname)->GetValuePointer();
    if (fDebug>=1)  cout << parname << " "<< tmpvar << endl;
  } else {
    cout << "*** ERROR!!! Could not find " << parname << " in the list of variables! ***" << endl;
  }
  return tmpvar;
}

//_____________________________________________________________________________
Int_t THcHodoscope::DefineIntVariable(const char* fName)
{
  // Define a variale of type int by looking it up in the THcParmList
  char prefix[2];
  char parname[100];
  Int_t tmpvar=-100000;
  prefix[0]=tolower(GetApparatus()->GetName()[0]);
  prefix[1]='\0';
  strcpy(parname,prefix);
  strcat(parname,fName);
  if (gHcParms->Find(parname)) {
    tmpvar=*(Int_t *)gHcParms->Find(parname)->GetValuePointer();
    if (fDebug>=1)  cout << parname << " "<< tmpvar << endl;
  } else {
    cout << "*** ERROR!!! Could not find " << parname << " in the list of variables! ***" << endl;
  }
  return tmpvar;
}

//_____________________________________________________________________________
void THcHodoscope::DefineArray(const char* fName, const Int_t index, Double_t *myArray)
{
  char prefix[2];
  char parname[100];
  //  Int_t tmpvar=-100000;
   prefix[0]=tolower(GetApparatus()->GetName()[0]);
  prefix[1]='\0';
  strcpy(parname,prefix);
  strcat(parname,fName);
  if (gHcParms->Find(parname)) {
    if (fDebug >=1) cout <<parname;
    Double_t* p = (Double_t *)gHcParms->Find(parname)->GetValuePointer();
    for(Int_t i=0;i<index;i++) {
      myArray[i] = p[i];
      if (fDebug>=1)    cout << " " << myArray[i];
    }
    if (fDebug>=1)  cout << endl;

  }
  else {
    cout <<" Could not find "<<parname<<" in the DataBase!!!\n";
  }
}

//_____________________________________________________________________________
void THcHodoscope::DefineArray(const char* fName, char** Suffix, const Int_t index, Double_t *myArray)
{
  // Try to read an array made up of what used to be (in the f77 days) a number of variables
  // example: hscin_1x_center, hscin_1y_center, hscin_2x_center, hscin_2y_center will become scin_center
  //
  char prefix[2];
  char parname[100],parname2[100];
  //  
  prefix[0]=tolower(GetApparatus()->GetName()[0]);
  prefix[1]='\0';
  strcpy(parname,prefix);
  strcat(parname,fName);
  for(Int_t i=0;i<index;i++) {
    strcpy(parname2,Form(parname,Suffix[i]));
    if (gHcParms->Find(parname2)) {
      if (fDebug >=1) cout <<parname2;
      myArray[i] = *(Double_t *)gHcParms->Find(parname2)->GetValuePointer();
      if (fDebug>=1)    cout << " " << myArray[i];
    }
    if (fDebug>=1)  cout << endl;
    else {
      cout <<" Could not find "<<parname2<<" in the DataBase!!!\n";
    }
  }
}

//_____________________________________________________________________________
void THcHodoscope::DefineArray(const char* fName, char** Suffix, const Int_t index, Int_t *myArray)
{
  // Try to read an array made up of what used to be (in the f77 days) a number of variables
  // example: hscin_1x_center, hscin_1y_center, hscin_2x_center, hscin_2y_center will become scin_center
  //
  char prefix[2];
  char parname[100],parname2[100];
  //  
  prefix[0]=tolower(GetApparatus()->GetName()[0]);
  prefix[1]='\0';
  strcpy(parname,prefix);
  strcat(parname,fName);
  for(Int_t i=0;i<index;i++) {
    strcpy(parname2,Form(parname,Suffix[i]));
    if (gHcParms->Find(parname2)) {
      if (fDebug >=1) cout <<parname2;
      myArray[i] = *(Int_t *)gHcParms->Find(parname2)->GetValuePointer();
      if (fDebug>=1)    cout << " " << myArray[i];
    }
    if (fDebug>=1)  cout << endl;
    else {
      cout <<" Could not find "<<parname2<<" in the DataBase!!!\n";
    }
  }
}

//_____________________________________________________________________________
Int_t THcHodoscope::ReadDatabase( const TDatime& date )
{

  // Read this detector's parameters from the database file 'fi'.
  // This function is called by THaDetectorBase::Init() once at the
  // beginning of the analysis.
  // 'date' contains the date/time of the run being analyzed.

  //  static const char* const here = "ReadDatabase()";
  char prefix[2];
  char parname[100];

  // Read data from database 
  // Pull values from the THcParmList instead of reading a database
  // file like Hall A does.

  // Will need to determine which spectrometer in order to construct
  // the parameter names (e.g. hscin_1x_nr vs. sscin_1x_nr)

  prefix[0]=tolower(GetApparatus()->GetName()[0]);
  //
  prefix[1]='\0';
  strcpy(parname,prefix);
  strcat(parname,"scin_");
  //  Int_t plen=strlen(parname);
  cout << " readdatabse hodo fnplanes = " << fNPlanes << endl;

  fNPaddle = new UInt_t [fNPlanes];
  fFPTime = new Double_t [fNPlanes];
  fPlaneCenter = new Double_t[fNPlanes];
  fPlaneSpacing = new Double_t[fNPlanes];

  prefix[0]=tolower(GetApparatus()->GetName()[0]);
  //
  prefix[1]='\0';

  for(UInt_t i=0;i<fNPlanes;i++) {
    
    DBRequest list[]={
      {Form("scin_%s_nr",fPlaneNames[i]), &fNPaddle[i], kInt},
      {0}
    };
    gHcParms->LoadParmValues((DBRequest*)&list,prefix);
  }

  // GN added
  // reading variables from *hodo.param
  fMaxScinPerPlane=fNPaddle[0];
  for (UInt_t i=1;i<fNPlanes;i++) {
    fMaxScinPerPlane=(fMaxScinPerPlane > fNPaddle[i])? fMaxScinPerPlane : fNPaddle[i];
  }
// need this for "padded arrays" i.e. 4x16 lists of parameters (GN)
  fMaxHodoScin=fMaxScinPerPlane*fNPlanes; 
  if (fDebug>=1)  cout <<"fMaxScinPerPlane = "<<fMaxScinPerPlane<<" fMaxHodoScin = "<<fMaxHodoScin<<endl;
  
  fHodoVelLight=new Double_t [fMaxHodoScin];
  fHodoPosSigma=new Double_t [fMaxHodoScin];
  fHodoNegSigma=new Double_t [fMaxHodoScin];
  fHodoPosMinPh=new Double_t [fMaxHodoScin];
  fHodoNegMinPh=new Double_t [fMaxHodoScin];
  fHodoPosPhcCoeff=new Double_t [fMaxHodoScin];
  fHodoNegPhcCoeff=new Double_t [fMaxHodoScin];
  fHodoPosTimeOffset=new Double_t [fMaxHodoScin];
  fHodoNegTimeOffset=new Double_t [fMaxHodoScin];
  fHodoPosPedLimit=new Int_t [fMaxHodoScin];
  fHodoNegPedLimit=new Int_t [fMaxHodoScin];
  fHodoPosInvAdcOffset=new Double_t [fMaxHodoScin];
  fHodoNegInvAdcOffset=new Double_t [fMaxHodoScin];
  fHodoPosInvAdcLinear=new Double_t [fMaxHodoScin];
  fHodoNegInvAdcLinear=new Double_t [fMaxHodoScin];
  fHodoPosInvAdcAdc=new Double_t [fMaxHodoScin];
  fHodoNegInvAdcAdc=new Double_t [fMaxHodoScin];
  


  prefix[1]='\0';
  DBRequest list[]={
    {"start_time_center",     &fStartTimeCenter,                      kDouble},
    {"start_time_slop",       &fStartTimeSlop,                        kDouble},
    {"scin_tdc_to_time",      &fScinTdcToTime,                        kDouble},
    {"scin_tdc_min",          &fScinTdcMin,                           kDouble},
    {"scin_tdc_max",          &fScinTdcMax,                           kDouble},
    {"tof_tolerance",         &fTofTolerance,          kDouble,         0,  1},
    {"pathlength_central",    &fPathLengthCentral,                    kDouble},
    {"hodo_vel_light",        &fHodoVelLight[0],       kDouble,  fMaxHodoScin},
    {"hodo_pos_sigma",        &fHodoPosSigma[0],       kDouble,  fMaxHodoScin},
    {"hodo_neg_sigma",        &fHodoNegSigma[0],       kDouble,  fMaxHodoScin},
    {"hodo_pos_minph",        &fHodoPosMinPh[0],       kDouble,  fMaxHodoScin},
    {"hodo_neg_minph",        &fHodoNegMinPh[0],       kDouble,  fMaxHodoScin},
    {"hodo_pos_phc_coeff",    &fHodoPosPhcCoeff[0],    kDouble,  fMaxHodoScin},
    {"hodo_neg_phc_coeff",    &fHodoNegPhcCoeff[0],    kDouble,  fMaxHodoScin},
    {"hodo_pos_time_offset",  &fHodoPosTimeOffset[0],  kDouble,  fMaxHodoScin},
    {"hodo_neg_time_offset",  &fHodoNegTimeOffset[0],  kDouble,  fMaxHodoScin},
    {"hodo_pos_ped_limit",    &fHodoPosPedLimit[0],    kInt,     fMaxHodoScin},
    {"hodo_neg_ped_limit",    &fHodoNegPedLimit[0],    kInt,     fMaxHodoScin},
    {"tofusinginvadc",        &fTofUsingInvAdc,        kInt,            0,  1},
    {0}
  };
  fTofUsingInvAdc = 0;		// Default if not defined
  fTofTolerance = 3.0;		// Default if not defined
  gHcParms->LoadParmValues((DBRequest*)&list,prefix);


  if (fTofUsingInvAdc) {
    DBRequest list2[]={
      {"hodo_pos_invadc_offset",&fHodoPosInvAdcOffset[0],kDouble,fMaxHodoScin},
      {"hodo_neg_invadc_offset",&fHodoNegInvAdcOffset[0],kDouble,fMaxHodoScin},
      {"hodo_pos_invadc_linear",&fHodoPosInvAdcLinear[0],kDouble,fMaxHodoScin},
      {"hodo_neg_invadc_linear",&fHodoNegInvAdcLinear[0],kDouble,fMaxHodoScin},
      {"hodo_pos_invadc_adc",&fHodoPosInvAdcAdc[0],kDouble,fMaxHodoScin},
      {"hodo_neg_invadc_adc",&fHodoNegInvAdcAdc[0],kDouble,fMaxHodoScin},
      {0}
    };
    gHcParms->LoadParmValues((DBRequest*)&list2,prefix);
  };
  if (fDebug >=1) {
    cout <<"******* Testing Hodoscope Parameter Reading ***\n";
    cout<<"StarTimeCenter = "<<fStartTimeCenter<<endl;
    cout<<"StartTimeSlop = "<<fStartTimeSlop<<endl;
    cout <<"ScintTdcToTime = "<<fScinTdcToTime<<endl;
    cout <<"TdcMin = "<<fScinTdcMin<<" TdcMax = "<<fScinTdcMax<<endl;
    cout <<"TofTolerance = "<<fTofTolerance<<endl;
    cout <<"*** VelLight ***\n";
    for (UInt_t i1=0;i1<fNPlanes;i1++) {
      cout<<"Plane "<<i1<<endl;
      for (UInt_t i2=0;i2<fMaxScinPerPlane;i2++) {
	cout<<fHodoVelLight[GetScinIndex(i1,i2)]<<" ";
      }
      cout <<endl;
    }
    cout <<endl<<endl;
    // check fHodoPosPhcCoeff
    /*
    cout <<"fHodoPosPhcCoeff = ";
    for (int i1=0;i1<fMaxHodoScin;i1++) {
      cout<<this->GetHodoPosPhcCoeff(i1)<<" ";
    }
    cout<<endl;
    */
  }
  //
  if ((fTofTolerance > 0.5) && (fTofTolerance < 10000.)) {
    cout << "USING "<<fTofTolerance<<" NSEC WINDOW FOR FP NO_TRACK CALCULATIONS.\n";
  }
  else {
    fTofTolerance= 3.0;
    cout << "*** USING DEFAULT 3 NSEC WINDOW FOR FP NO_TRACK CALCULATIONS!! ***\n";
  }
  fIsInit = true;
  return kOK;
}

//_____________________________________________________________________________
Int_t THcHodoscope::DefineVariables( EMode mode )
{
  // Initialize global variables and lookup table for decoder
  cout << "THcHodoscope::DefineVariables called " << GetName() << endl;
  if( mode == kDefine && fIsSetup ) return kOK;
  fIsSetup = ( mode == kDefine );

  // Register variables in global list

  RVarDef vars[] = {
    // Move these into THcHallCSpectrometer using track fTracks
    //    {"fpBeta",          "Beta of the track",                    "fBeta"},
    //    {"fpBetaChisq",     "Chi square of the track",              "fBetaChisq"},
    {"fpHitsTime",      "Time at focal plane from all hits",    "fFPTime"},
    {"starttime",       "Hodoscope Start Time",                 "fStartTime"},
    {"hgoodstarttime",  "Hodoscope Good Start Time",            "fGoodStartTime"},
    //    { "nlthit", "Number of Left paddles TDC times",  "fLTNhit" },
    //    { "nrthit", "Number of Right paddles TDC times", "fRTNhit" },
    //    { "nlahit", "Number of Left paddles ADCs amps",  "fLANhit" },
    //    { "nrahit", "Number of Right paddles ADCs amps", "fRANhit" },
    //    { "lt",     "TDC values left side",              "fLT" },
    //    { "lt_c",   "Corrected times left side",         "fLT_c" },
    //    { "rt",     "TDC values right side",             "fRT" },
    //    { "rt_c",   "Corrected times right side",        "fRT_c" },
    //    { "la",     "ADC values left side",              "fLA" },
    //    { "la_p",   "Corrected ADC values left side",    "fLA_p" },
    //    { "la_c",   "Corrected ADC values left side",    "fLA_c" },
    //    { "ra",     "ADC values right side",             "fRA" },
    //    { "ra_p",   "Corrected ADC values right side",   "fRA_p" },
    //    { "ra_c",   "Corrected ADC values right side",   "fRA_c" },
    //    { "nthit",  "Number of paddles with l&r TDCs",   "fNhit" },
    //    { "t_pads", "Paddles with l&r coincidence TDCs", "fHitPad" },
    //    { "y_t",    "y-position from timing (m)",        "fYt" },
    //    { "y_adc",  "y-position from amplitudes (m)",    "fYa" },
    //    { "time",   "Time of hit at plane (s)",          "fTime" },
    //    { "dtime",  "Est. uncertainty of time (s)",      "fdTime" },
    //    { "dedx",   "dEdX-like deposited in paddle",     "fdEdX" },
    // In hphysics will put the dedx for each plane from the best track into hist 
    //    { "troff",  "Trigger offset for paddles",        "fTrigOff"},
    //    { "trn",    "Number of tracks for hits",         "GetNTracks()" },
    //    { "trx",    "x-position of track in det plane",  "fTrackProj.THaTrackProj.fX" },
    //    { "try",    "y-position of track in det plane",  "fTrackProj.THaTrackProj.fY" },
    //    { "trpath", "TRCS pathlen of track to det plane","fTrackProj.THaTrackProj.fPathl" },
    //    { "trdx",   "track deviation in x-position (m)", "fTrackProj.THaTrackProj.fdX" },
    //    { "trpad",  "paddle-hit associated with track",  "fTrackProj.THaTrackProj.fChannel" },
    { 0 }
  };
  return DefineVarsFromList( vars, mode );
  //  return kOK;
}

//_____________________________________________________________________________
THcHodoscope::~THcHodoscope()
{
  // Destructor. Remove variables from global list.

  delete [] fFPTime;
  delete [] fPlaneCenter;
  delete [] fPlaneSpacing;

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
void THcHodoscope::DeleteArrays()
{
  // Delete member arrays. Used by destructor.
  for(UInt_t k = 0; k < fNPlanes; k++){
    delete [] fScinHit[k];
  }
  delete [] fScinHit;
  
  delete [] fNPaddle;             fNPaddle = NULL;
  delete [] fHodoVelLight;        fHodoVelLight = NULL;
  delete [] fHodoPosSigma;        fHodoPosSigma = NULL;
  delete [] fHodoNegSigma;        fHodoNegSigma = NULL;
  delete [] fHodoPosMinPh;        fHodoPosMinPh = NULL;
  delete [] fHodoNegMinPh;        fHodoNegMinPh = NULL;
  delete [] fHodoPosPhcCoeff;     fHodoPosPhcCoeff = NULL;
  delete [] fHodoNegPhcCoeff;     fHodoNegPhcCoeff = NULL;
  delete [] fHodoPosTimeOffset;   fHodoPosTimeOffset = NULL;
  delete [] fHodoNegTimeOffset;   fHodoNegTimeOffset = NULL;
  delete [] fHodoPosPedLimit;     fHodoPosPedLimit = NULL;
  delete [] fHodoNegPedLimit;     fHodoNegPedLimit = NULL;
  delete [] fHodoPosInvAdcOffset; fHodoPosInvAdcOffset = NULL;
  delete [] fHodoNegInvAdcOffset; fHodoNegInvAdcOffset = NULL;
  delete [] fHodoPosInvAdcLinear; fHodoPosInvAdcLinear = NULL;
  delete [] fHodoNegInvAdcLinear; fHodoNegInvAdcLinear = NULL;
  delete [] fHodoPosInvAdcAdc;    fHodoPosInvAdcAdc = NULL;

  delete [] fGoodPlaneTime;       fGoodPlaneTime = NULL;     // Ahmed
  delete [] fNPlaneTime;          fNPlaneTime = NULL;        // Ahmed
  delete [] fSumPlaneTime;        fSumPlaneTime = NULL;      // Ahmed

  delete [] fNScinHits;           fNScinHits = NULL;         // Ahmed

  //  delete [] fSpacing; fSpacing = NULL;
  //delete [] fCenter;  fCenter = NULL; // This 2D. What is correct way to delete?

  //  delete [] fRA_c;    fRA_c    = NULL;
  //  delete [] fRA_p;    fRA_p    = NULL;
  //  delete [] fRA;      fRA      = NULL;
  //  delete [] fLA_c;    fLA_c    = NULL;
  //  delete [] fLA_p;    fLA_p    = NULL;
  //  delete [] fLA;      fLA      = NULL;
  //  delete [] fRT_c;    fRT_c    = NULL;
  //  delete [] fRT;      fRT      = NULL;
  //  delete [] fLT_c;    fLT_c    = NULL;
  //  delete [] fLT;      fLT      = NULL;
  
  //  delete [] fRGain;   fRGain   = NULL;
  //  delete [] fLGain;   fLGain   = NULL;
  //  delete [] fRPed;    fRPed    = NULL;
  //  delete [] fLPed;    fLPed    = NULL;
  //  delete [] fROff;    fROff    = NULL;
  //  delete [] fLOff;    fLOff    = NULL;
  //  delete [] fTWalkPar; fTWalkPar = NULL;
  //  delete [] fTrigOff; fTrigOff = NULL;

  //  delete [] fHitPad;  fHitPad  = NULL;
  //  delete [] fTime;    fTime    = NULL;
  //  delete [] fdTime;   fdTime   = NULL;
  //  delete [] fYt;      fYt      = NULL;
  //  delete [] fYa;      fYa      = NULL;
}

//_____________________________________________________________________________
inline 
void THcHodoscope::ClearEvent()
{
  // Reset per-event data.

  //  for ( Int_t imaxhit = 0; imaxhit < MAXHODHITS; imaxhit++ ){
  //    fBeta[imaxhit] = 0.;
  //    fBetaChisq[imaxhit] = 0.;
  //  }

  for(UInt_t ip=0;ip<fNPlanes;ip++) {
    fPlanes[ip]->Clear();
    fFPTime[ip]=0.;
    fPlaneCenter[ip]=0.;
    fPlaneSpacing[ip]=0.;
  }
  fdEdX.clear();
  fNScinHit.clear();
}

//_____________________________________________________________________________
Int_t THcHodoscope::Decode( const THaEvData& evdata )
{
  ClearEvent();
  // Get the Hall C style hitlist (fRawHitList) for this event
  Int_t nhits = DecodeToHitList(evdata);
  //
  // GN: print event number so we can cross-check with engine
  // if (evdata.GetEvNum()>1000) 
  //   cout <<"\nhcana_event " << evdata.GetEvNum()<<endl;
  
  //  fCheckEvent = evdata.GetEvNum();

  if(gHaCuts->Result("Pedestal_event")) {
    Int_t nexthit = 0;
    for(UInt_t ip=0;ip<fNPlanes;ip++) {
            
      nexthit = fPlanes[ip]->AccumulatePedestals(fRawHitList, nexthit);
    }
    fAnalyzePedestals = 1;	// Analyze pedestals first normal events
    return(0);
  }
  if(fAnalyzePedestals) {
    for(UInt_t ip=0;ip<fNPlanes;ip++) {
      
      fPlanes[ip]->CalculatePedestals();
    }
    fAnalyzePedestals = 0;	// Don't analyze pedestals next event
  }

  // Let each plane get its hits
  Int_t nexthit = 0;

  fStartTime=0;
  fNfptimes=0;
  for(UInt_t ip=0;ip<fNPlanes;ip++) {

    fPlaneCenter[ip] = fPlanes[ip]->GetPosCenter(0) + fPlanes[ip]->GetPosOffset();
    fPlaneSpacing[ip] = fPlanes[ip]->GetSpacing();
    
    //    nexthit = fPlanes[ip]->ProcessHits(fRawHitList, nexthit);
    // GN: select only events that have reasonable TDC values to start with
    // as per the Engine h_strip_scin.f
    nexthit = fPlanes[ip]->ProcessHits(fRawHitList,nexthit);
    if (fPlanes[ip]->GetNScinHits()>0) {
      fPlanes[ip]->PulseHeightCorrection();
      // GN: allow for more than one fptime per plane!!
      for (Int_t i=0;i<fPlanes[ip]->GetNScinGoodHits();i++) {
	if (TMath::Abs(fPlanes[ip]->GetFpTime(i)-fStartTimeCenter)<=fStartTimeSlop) {
	  fStartTime=fStartTime+fPlanes[ip]->GetFpTime(i);
	  // GN write stuff out so I can compare with engine
	  ///	  cout<<"hcana event= "<<evdata.GetEvNum()<<" fNfptimes= "<<fNfptimes<<" fptime= "<<fPlanes[ip]->GetFpTime(i)<<endl;
	  fNfptimes++;
	}
      }
    }
  }
  if (fNfptimes>0) {
    fStartTime=fStartTime/fNfptimes;
    fGoodStartTime=kTRUE;
  } else {
    fGoodStartTime=kFALSE;
    fStartTime=fStartTimeCenter;
  }
#if 0
  for(Int_t ihit = 0; ihit < fNRawHits ; ihit++) {
    THcRawHodoHit* hit = (THcRawHodoHit *) fRawHitList->At(ihit);
    cout << ihit << " : " << hit->fPlane << ":" << hit->fCounter << " : "
	 << hit->fADC_pos << " " << hit->fADC_neg << " "  <<  hit->fTDC_pos
	 << " " <<  hit->fTDC_neg << endl;
  }
  cout << endl;
#endif
  ///  fStartTime = 500;		// Drift Chamber will need this

  return nhits;
}

//_____________________________________________________________________________
Int_t THcHodoscope::ApplyCorrections( void )
{
  return(0);
}
//_____________________________________________________________________________
Double_t THcHodoscope::TimeWalkCorrection(const Int_t& paddle,
					     const ESide side)
{
  return(0.0);
}

//_____________________________________________________________________________
Int_t THcHodoscope::CoarseProcess( TClonesArray&  tracks  )
{

  ApplyCorrections();
 
  return 0;
}

//_____________________________________________________________________________
Int_t THcHodoscope::FineProcess( TClonesArray& tracks )
{

  Int_t Ntracks = tracks.GetLast()+1; // Number of reconstructed tracks
  Int_t fJMax, fMaxHit;
  Int_t fRawIndex = -1;
  Double_t fScinTrnsCoord, fScinLongCoord, fScinCenter, fSumfpTime;
  Double_t fP, fXcoord, fYcoord, fTMin;
  // -------------------------------------------------

  Double_t hpartmass=0.00051099; // Fix it
 
  if (tracks.GetLast()+1 > 0 ) {

    // **MAIN LOOP: Loop over all tracks and get corrected time, tof, beta...
    Double_t* fNPmtHit = new Double_t [Ntracks];
    Double_t* fTimeAtFP = new Double_t [Ntracks];
    for ( Int_t itrack = 0; itrack < Ntracks; itrack++ ) { // Line 133
      fNPmtHit[itrack]=0;
      fTimeAtFP[itrack]=0;

      THaTrack* theTrack = dynamic_cast<THaTrack*>( tracks.At(itrack) );
      if (!theTrack) return -1;
      
      for ( UInt_t ip = 0; ip < fNPlanes; ip++ ){ 
	fGoodPlaneTime[ip] = kFALSE; 
	fNScinHits[ip] = 0;
	fNPlaneTime[ip] = 0;
	fSumPlaneTime[ip] = 0.;
      }
      std::vector<Double_t> dedx_temp;
      fdEdX.push_back(dedx_temp); // Create array of dedx per hit
      
      Int_t fNfpTime = 0;
      Double_t betaChisq = -3;
      Double_t beta = 0;
      //      fTimeAtFP[itrack] = 0.;
      fSumfpTime = 0.; // Line 138
      fNScinHit.push_back(0);
      fP = theTrack->GetP(); // Line 142 
      Double_t betaP = fP/( TMath::Sqrt( fP * fP + hpartmass * hpartmass) );
      
      //! Calculate all corrected hit times and histogram
      //! This uses a copy of code below. Results are save in time_pos,neg
      //! including the z-pos. correction assuming nominal value of betap
      //! Code is currently hard-wired to look for a peak in the
      //! range of 0 to 100 nsec, with a group of times that all
      //! agree withing a time_tolerance of time_tolerance nsec. The normal
      //! peak position appears to be around 35 nsec.
      //! NOTE: if want to find farticles with beta different than
      //! reference particle, need to make sure this is big enough
      //! to accomodate difference in TOF for other particles
      //! Default value in case user hasnt definedd something reasonable
      // Line 162 to 171 is already done above in ReadDatabase
      
      for (Int_t j=0; j<200; j++) { fTimeHist[j]=0; } // Line 176
      
      // Loop over scintillator planes.
      // In ENGINE, its loop over good scintillator hits.
      
      fTOFCalc.clear();
      Int_t ihhit = 0;		// Hit # overall
      for( UInt_t ip = 0; ip < fNPlanes; ip++ ) {
	
	fNScinHits[ip] = fPlanes[ip]->GetNScinHits();

	// first loop over hits with in a single plane
	fTOFPInfo.clear();
	for ( Int_t iphit = 0; iphit < fNScinHits[ip]; iphit++ ){
	  // iphit is hit # within a plane
	  	  
	  fTOFPInfo.push_back(TOFPInfo());
	  // Can remove these as we will initialize in the constructor
	  fTOFPInfo[iphit].time_pos = -99.0;
	  fTOFPInfo[iphit].time_neg = -99.0;
	  fTOFPInfo[iphit].keep_pos = kFALSE;
	  fTOFPInfo[iphit].keep_neg = kFALSE;
	  fTOFPInfo[iphit].scin_pos_time = 0.0;
	  fTOFPInfo[iphit].scin_neg_time = 0.0;
	  
	  scinPosADC = fPlanes[ip]->GetPosADC();
	  scinNegADC = fPlanes[ip]->GetNegADC();
	  scinPosTDC = fPlanes[ip]->GetPosTDC();
	  scinNegTDC = fPlanes[ip]->GetNegTDC();  
	  	  
	  Int_t paddle = ((THcSignalHit*)scinPosTDC->At(iphit))->GetPaddleNumber()-1;
	  
	  fXcoord = theTrack->GetX() + theTrack->GetTheta() *
	    ( fPlanes[ip]->GetZpos() +
	      ( paddle % 2 ) * fPlanes[ip]->GetDzpos() ); // Line 183
	  
	  fYcoord = theTrack->GetY() + theTrack->GetPhi() *
	    ( fPlanes[ip]->GetZpos() +
	      ( paddle % 2 ) * fPlanes[ip]->GetDzpos() ); // Line 184
	  	  
	  if ( ( ip == 0 ) || ( ip == 2 ) ){ // !x plane. Line 185
	    fScinTrnsCoord = fXcoord;
	    fScinLongCoord = fYcoord;
	  }
	  
	  else if ( ( ip == 1 ) || ( ip == 3 ) ){ // !y plane. Line 188
	    fScinTrnsCoord = fYcoord;
	    fScinLongCoord = fXcoord;
	  }
	  else { return -1; } // Line 195
	  
	  fScinCenter = fPlanes[ip]->GetPosCenter(paddle) + fPlanes[ip]->GetPosOffset();

	  // Index to access the 2d arrays of paddle/scintillator properties
	  Int_t pindex = fNPlanes * paddle + ip;
	  

	  if ( TMath::Abs( fScinCenter - fScinTrnsCoord ) <
	       ( fPlanes[ip]->GetSize() * 0.5 + fPlanes[ip]->GetHodoSlop() ) ){ // Line 293
	    
	    if ( ( ((THcSignalHit*)scinPosTDC->At(iphit))->GetData() > fScinTdcMin ) &&
		 ( ((THcSignalHit*)scinPosTDC->At(iphit))->GetData() < fScinTdcMax ) ) { // Line 199
	      
	      Double_t adcPh = ((THcSignalHit*)scinPosADC->At(iphit))->GetData();
	      fTOFPInfo[iphit].adcPh = adcPh;
	      Double_t path = fPlanes[ip]->GetPosLeft() - fScinLongCoord;
	      fTOFPInfo[iphit].path = path;
	      Double_t time = ((THcSignalHit*)scinPosTDC->At(iphit))->GetData() * fScinTdcToTime;
	      time = time - fHodoPosPhcCoeff[pindex] *
		TMath::Sqrt( TMath::Max( 0., ( ( adcPh / fHodoPosMinPh[pindex] ) - 1 ) ) );
	      time = time - ( path / fHodoVelLight[pindex] ) - ( fPlanes[ip]->GetZpos() +  
								 ( paddle % 2 ) * fPlanes[ip]->GetDzpos() ) / ( 29.979 * betaP ) *
		TMath::Sqrt( 1. + theTrack->GetTheta() * theTrack->GetTheta() +
			     theTrack->GetPhi() * theTrack->GetPhi() );
	      fTOFPInfo[iphit].time = time;
	      fTOFPInfo[iphit].time_pos = time - fHodoPosTimeOffset[pindex];
	      
	      for ( Int_t k = 0; k < 200; k++ ){ // Line 211
		fTMin = 0.5 * ( k + 1 ) ;
		if ( ( fTOFPInfo[iphit].time_pos > fTMin ) && ( fTOFPInfo[iphit].time_pos < ( fTMin + fTofTolerance ) ) )
		  fTimeHist[k] ++;
	      }
	    } // TDC pos hit condition
	    
	    
	    if ( ( ((THcSignalHit*)scinNegTDC->At(iphit))->GetData() > fScinTdcMin ) &&
		 ( ((THcSignalHit*)scinNegTDC->At(iphit))->GetData() < fScinTdcMax ) ) { // Line 218
	      
	      Double_t adcPh = ((THcSignalHit*)scinNegADC->At(iphit))->GetData();
	      fTOFPInfo[iphit].adcPh = adcPh;
	      Double_t path =  fScinLongCoord - fPlanes[ip]->GetPosRight();
	      fTOFPInfo[iphit].path = path;
	      Double_t time = ((THcSignalHit*)scinNegTDC->At(iphit))->GetData() * fScinTdcToTime;
	      time =time - fHodoNegPhcCoeff[pindex] * 
		TMath::Sqrt( TMath::Max( 0., ( ( adcPh / fHodoNegMinPh[pindex] ) - 1 ) ) );
	      time = time - ( path / fHodoVelLight[pindex] ) - ( fPlanes[ip]->GetZpos() +
								 ( paddle % 2 ) * fPlanes[ip]->GetDzpos() ) / ( 29.979 * betaP ) *
		TMath::Sqrt( 1. + theTrack->GetTheta() * theTrack->GetTheta() +
			     theTrack->GetPhi() * theTrack->GetPhi() );
	      fTOFPInfo[iphit].time = time;
	      fTOFPInfo[iphit].time_neg = time - fHodoNegTimeOffset[pindex];
	      
	      for ( Int_t k = 0; k < 200; k++ ){ // Line 230
		fTMin = 0.5 * ( k + 1 );
		if ( ( fTOFPInfo[iphit].time_neg > fTMin ) && ( fTOFPInfo[iphit].time_neg < ( fTMin + fTofTolerance ) ) )
		  fTimeHist[k] ++;
	      }
	    } // TDC neg hit condition
	  } // condition for cenetr on a paddle
	} // First loop over hits in a plane <---------

	//-----------------------------------------------------------------------------------------------
	//------------- First large loop over scintillator hits in a plane ends here --------------------
	//-----------------------------------------------------------------------------------------------
	
	fJMax = 0; // Line 240
	fMaxHit = 0;
	
	for ( Int_t k = 0; k < 200; k++ ){
	  if ( fTimeHist[k] > fMaxHit ){
	    fJMax = k+1;
	    fMaxHit = fTimeHist[k];
	  }
	}
	
	if ( fJMax >= 0 ){ // Line 248. Here I followed the code of THcSCintilaltorPlane::PulseHeightCorrection
	  fTMin = 0.5 * fJMax;
	  for( Int_t iphit = 0; iphit < fNScinHits[ip]; iphit++) { // Loop over sinc. hits. in plane
	    if ( ( fTOFPInfo[iphit].time_pos > fTMin ) && ( fTOFPInfo[iphit].time_pos < ( fTMin + fTofTolerance ) ) ) {
	      fTOFPInfo[iphit].keep_pos=kTRUE;
	    }	
	    if ( ( fTOFPInfo[iphit].time_neg > fTMin ) && ( fTOFPInfo[iphit].time_neg < ( fTMin + fTofTolerance ) ) ){
	      fTOFPInfo[iphit].keep_neg=kTRUE;
	    }	
	  }
	} // fJMax > 0 condition
	
	//---------------------------------------------------------------------------------------------	
	// ---------------------- Scond loop over scint. hits in a plane ------------------------------
	//---------------------------------------------------------------------------------------------

	for ( Int_t iphit = 0; iphit < fNScinHits[ip]; iphit++ ){
	  
	  fTOFCalc.push_back(TOFCalc());
	  // Do we set back to false for each track, or just once per event?
	  fTOFCalc[ihhit].good_scin_time = kFALSE;
	  // These need a track index too to calculate efficiencies
	  fTOFCalc[ihhit].good_tdc_pos = kFALSE;
	  fTOFCalc[ihhit].good_tdc_neg = kFALSE;

	  //	  ihhit ++;
	  //	  fRawIndex ++;   // Is fRawIndex ever different from ihhit
	  fRawIndex = ihhit;

	  Int_t paddle = ((THcSignalHit*)scinPosTDC->At(iphit))->GetPaddleNumber()-1;
	  fTOFCalc[ihhit].hit_paddle = paddle;
	  fTOFCalc[fRawIndex].good_raw_pad = paddle;
	  
	  fXcoord = theTrack->GetX() + theTrack->GetTheta() *
	    ( fPlanes[ip]->GetZpos() + ( paddle % 2 ) * fPlanes[ip]->GetDzpos() ); // Line 277
	  fYcoord = theTrack->GetY() + theTrack->GetPhi() *
	    ( fPlanes[ip]->GetZpos() + ( paddle % 2 ) * fPlanes[ip]->GetDzpos() ); // Line 278
	  
	  
	  if ( ( ip == 0 ) || ( ip == 2 ) ){ // !x plane. Line 278
	    fScinTrnsCoord = fXcoord;
	    fScinLongCoord = fYcoord;
	  }
	  else if ( ( ip == 1 ) || ( ip == 3 ) ){ // !y plane. Line 281
	    fScinTrnsCoord = fYcoord;
	    fScinLongCoord = fXcoord;
	  }
	  else { return -1; } // Line 288
	  
	  fScinCenter = fPlanes[ip]->GetPosCenter(paddle) + fPlanes[ip]->GetPosOffset();
	  Int_t pindex = fNPlanes * paddle + ip;
	  
	  // ** Check if scin is on track
	  if ( TMath::Abs( fScinCenter - fScinTrnsCoord ) >
	       ( fPlanes[ip]->GetSize() * 0.5 + fPlanes[ip]->GetHodoSlop() ) ){ // Line 293
	    //	    scinOnTrack[itrack][iphit] = kFALSE;
	  }
	  else{
	    //	    scinOnTrack[itrack][iphit] = kTRUE;
	    
	    // * * Check for good TDC
	    if ( ( ((THcSignalHit*)scinPosTDC->At(iphit))->GetData() > fScinTdcMin ) &&
		 ( ((THcSignalHit*)scinPosTDC->At(iphit))->GetData() < fScinTdcMax ) &&
		 ( fTOFPInfo[iphit].keep_pos ) ) { // 301
	      
	      // ** Calculate time for each tube with a good tdc. 'pos' side first.
	      fTOFCalc[ihhit].good_tdc_pos = kTRUE;
	      //	      fNtof ++;
	      Double_t adcPh = ((THcSignalHit*)scinPosADC->At(iphit))->GetData();
	      fTOFPInfo[iphit].adcPh = adcPh;
	      Double_t path = fPlanes[ip]->GetPosLeft() - fScinLongCoord;
	      fTOFPInfo[iphit].path = path;
	      
	      // * Convert TDC value to time, do pulse height correction, correction for
	      // * propogation of light thru scintillator, and offset.
	      
	      Double_t time = ((THcSignalHit*)scinPosTDC->At(iphit))->GetData() * fScinTdcToTime;
	      time = time - ( fHodoPosPhcCoeff[pindex] * TMath::Sqrt( TMath::Max( 0. , 
					( ( adcPh / fHodoPosMinPh[pindex] ) - 1 ) ) ) );
	      time = time - ( path / fHodoVelLight[pindex] );
	      fTOFPInfo[iphit].time = time;
	      fTOFPInfo[iphit].scin_pos_time = time - fHodoPosTimeOffset[pindex];
	      
	    } // check for good pos TDC condition
	    
	    // ** Repeat for pmts on 'negative' side
	    if ( ( ((THcSignalHit*)scinNegTDC->At(iphit))->GetData() > fScinTdcMin ) &&
		 ( ((THcSignalHit*)scinNegTDC->At(iphit))->GetData() < fScinTdcMax ) &&
		 ( fTOFPInfo[iphit].keep_neg ) ) { //
	      
	      // ** Calculate time for each tube with a good tdc. 'pos' side first.
	      fTOFCalc[ihhit].good_tdc_neg = kTRUE;
	      //	      fNtof ++;
	      Double_t adcPh = ((THcSignalHit*)scinNegADC->At(iphit))->GetData();
	      fTOFPInfo[iphit].adcPh = adcPh;
	      Double_t path = fPlanes[ip]->GetPosRight() - fScinLongCoord;
	      fTOFPInfo[iphit].path = path;
	      
	      // * Convert TDC value to time, do pulse height correction, correction for
	      // * propogation of light thru scintillator, and offset.
	      Double_t time = ((THcSignalHit*)scinNegTDC->At(iphit))->GetData() * fScinTdcToTime;
	      time = time - ( fHodoNegPhcCoeff[pindex] *
			   TMath::Sqrt( TMath::Max( 0. , ( ( adcPh / fHodoNegMinPh[pindex] ) - 1 ) ) ) );
	      time = time - ( path / fHodoVelLight[pindex] );
	      fTOFPInfo[iphit].time = time;
	      fTOFPInfo[iphit].scin_neg_time = time - fHodoNegTimeOffset[pindex];
	      
	    } // check for good neg TDC condition
	    
	    // ** Calculate ave time for scin and error.
	    if ( fTOFCalc[ihhit].good_tdc_pos ){
	      if ( fTOFCalc[ihhit].good_tdc_neg ){	
		fTOFCalc[ihhit].scin_time  = ( fTOFPInfo[iphit].scin_pos_time + fTOFPInfo[iphit].scin_neg_time ) / 2.;
		fTOFCalc[ihhit].scin_sigma = TMath::Sqrt( fHodoPosSigma[pindex] * fHodoPosSigma[pindex] + 
							  fHodoNegSigma[pindex] * fHodoNegSigma[pindex] )/2.;
		fTOFCalc[ihhit].good_scin_time = kTRUE;
		//		fNtofPairs ++;
	      }
	      else{
		fTOFCalc[ihhit].scin_time = fTOFPInfo[iphit].scin_pos_time;
		fTOFCalc[ihhit].scin_sigma = fHodoPosSigma[pindex];
		fTOFCalc[ihhit].good_scin_time = kTRUE;
	      }
	    }
	    else {
	      if ( fTOFCalc[ihhit].good_tdc_neg ){
		fTOFCalc[ihhit].scin_time = fTOFPInfo[iphit].scin_neg_time;
		fTOFCalc[ihhit].scin_sigma = fHodoNegSigma[pindex];
		fTOFCalc[ihhit].good_scin_time = kTRUE;
	      }
	    } // In h_tof.f this includes the following if condition for time at focal plane
	    // // because it is written in FORTRAN code

	    // c     Get time at focal plane
	    if ( fTOFCalc[ihhit].good_scin_time ){
	      
	      // scin_time_fp doesn't need to be an array
	      Double_t scin_time_fp = fTOFCalc[ihhit].scin_time -
	       	( fPlanes[ip]->GetZpos() + ( paddle % 2 ) * fPlanes[ip]->GetDzpos() ) /
	       	( 29.979 * betaP ) *
	       	TMath::Sqrt( 1. + theTrack->GetTheta() * theTrack->GetTheta() +
	       		     theTrack->GetPhi() * theTrack->GetPhi() );

	      // ---------------------------------------------------------------------------
	      // Date: July 8 2014
	      //
	      // Right now we do not need this code for beta and chisquare
	      //
	      //
	      fSumfpTime = fSumfpTime + scin_time_fp;
	      fNfpTime ++;
	      //
	      // ---------------------------------------------------------------------------

	      fSumPlaneTime[ip] = fSumPlaneTime[ip] + scin_time_fp;
	      fNPlaneTime[ip] ++;
	      fNScinHit[itrack] ++;
	      //	      scinHit[itrack][fNScinHit[itrack]] = iphit;
	      //	      scinfFPTime[itrack][fNScinHit[itrack]] = fScinTimefp[iphit];
	      
	      // ---------------------------------------------------------------------------
	      // Date: July 8 2014
	      // This counts the pmt hits. Right now we don't need it so it is commentd off
	      //
	      if ( ( fTOFCalc[ihhit].good_tdc_pos ) && ( fTOFCalc[ihhit].good_tdc_neg ) ){
	      	fNPmtHit[itrack] = fNPmtHit[itrack] + 2;
	      }
	      else {
	      	fNPmtHit[itrack] = fNPmtHit[itrack] + 1;
	      }
	      // ---------------------------------------------------------------------------


	      //	      fdEdX[itrack][iphit] = 5.0;


	      // --------------------------------------------------------------------------------------------
	      // Date: July 8 201  May be we need this, not sure.
	      //

	      fdEdX[itrack].push_back(0.0);
	      
	      if ( fTOFCalc[ihhit].good_tdc_pos ){
		if ( fTOFCalc[ihhit].good_tdc_neg ){
		  fdEdX[itrack][fNScinHit[itrack]-1]=
		    TMath::Sqrt( TMath::Max( 0., ((THcSignalHit*)scinPosADC->At(iphit))->GetData() *
                                                 ((THcSignalHit*)scinNegADC->At(iphit))->GetData() ) );
		}
		else{
		  fdEdX[itrack][fNScinHit[itrack]-1]=
		    TMath::Max( 0., ((THcSignalHit*)scinPosADC->At(iphit))->GetData() );
	       	}
	      }
	      else{
		if ( fTOFCalc[ihhit].good_tdc_neg ){
		  fdEdX[itrack][fNScinHit[itrack]-1]=
		    TMath::Max( 0., ((THcSignalHit*)scinNegADC->At(iphit))->GetData() );
		}
		else{
		  fdEdX[itrack][fNScinHit[itrack]-1]=0.0;
		}
	      }
	      // --------------------------------------------------------------------------------------------


	    } // time at focal plane condition
	  } // on track else condition
	  
	  // ** See if there are any good time measurements in the plane.
	  if ( fTOFCalc[ihhit].good_scin_time ){
	    fGoodPlaneTime[ip] = kTRUE;
	  }

	  // Can this be done after looping over hits and planes?
	  if ( fGoodPlaneTime[2] )	theTrack->SetGoodPlane3( 1 );
	  if ( !fGoodPlaneTime[2] )	theTrack->SetGoodPlane3( 0 );
	  if ( fGoodPlaneTime[3] )	theTrack->SetGoodPlane4( 1 );
	  if ( !fGoodPlaneTime[3] )	theTrack->SetGoodPlane4( 0 );

	  ihhit ++;

	} // Second loop over hits of a scintillator plane ends here
      } // Loop over scintillator planes ends here

      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------
      //------------------------------------------------------------------------------

      // * * Fit beta if there are enough time measurements (one upper, one lower)
      // From h_tof_fit
      if ( ( ( fGoodPlaneTime[0] ) || ( fGoodPlaneTime[1] ) ) && 
	   ( ( fGoodPlaneTime[2] ) || ( fGoodPlaneTime[3] ) ) ){	
	
	Double_t sumw, sumt, sumz, sumzz, sumtz;
	Double_t scinWeight, tmp, t0, tmpDenom, pathNorm, zPosition, timeDif;
	
	sumw = 0.;	sumt = 0.;	sumz = 0.;	sumzz = 0.;	sumtz = 0.;
	
	ihhit = 0;  
	for ( UInt_t ip = 0; ip < fNPlanes; ip++ ){

	  if (!fPlanes[ip])
	    return -1;
	  
	  fNScinHits[ip] = fPlanes[ip]->GetNScinHits();	  
	  for (Int_t iphit = 0; iphit < fNScinHits[ip]; iphit++ ){
	    
	    if ( fTOFCalc[ihhit].good_scin_time ) {
	      
	      scinWeight = 1 / ( fTOFCalc[ihhit].scin_sigma * fTOFCalc[ihhit].scin_sigma );
	      zPosition = ( fPlanes[ip]->GetZpos() + ( fTOFCalc[ihhit].hit_paddle % 2 ) * fPlanes[ip]->GetDzpos() );
	      
	      sumw += scinWeight;
	      sumt += scinWeight * fTOFCalc[ihhit].scin_time;
	      sumz += scinWeight * zPosition;
	      sumzz += scinWeight * ( zPosition * zPosition );
	      sumtz += scinWeight * zPosition * fTOFCalc[ihhit].scin_time;
	      
	      	      
	    } // condition of good scin time
	    ihhit ++;
	  } // loop over hits of plane
	} // loop over planes

	tmp = sumw * sumzz - sumz * sumz ;
	t0 = ( sumt * sumzz - sumz * sumtz ) / tmp ;
	tmpDenom = sumw * sumtz - sumz * sumt;

	if ( TMath::Abs( tmpDenom ) > ( 1 / 10000000000.0 ) ) {
	  
	  beta = tmp / tmpDenom;
	  betaChisq = 0.;
	  
	  ihhit = 0;
	  for ( UInt_t ip = 0; ip < fNPlanes; ip++ ){                           // Loop over planes
	    if (!fPlanes[ip])
	      return -1;
	    
	    fNScinHits[ip] = fPlanes[ip]->GetNScinHits();	  
	    for (Int_t iphit = 0; iphit < fNScinHits[ip]; iphit++ ){                    // Loop over hits of a plane
	      
	      if ( fTOFCalc[ihhit].good_scin_time ){
		
		zPosition = ( fPlanes[ip]->GetZpos() + ( fTOFCalc[ihhit].hit_paddle % 2 ) * fPlanes[ip]->GetDzpos() );
		timeDif = ( fTOFCalc[ihhit].scin_time - t0 );
		
		betaChisq += ( ( zPosition / beta - timeDif ) * ( zPosition / beta - timeDif ) )  / 
		  ( fTOFCalc[ihhit].scin_sigma * fTOFCalc[ihhit].scin_sigma );
		
	      } // condition for good scin time
	      ihhit++;
	    } // loop over hits of a plane
	  } // loop over planes
	  
	  pathNorm = TMath::Sqrt( 1. + theTrack->GetTheta() * theTrack->GetTheta() + theTrack->GetPhi() * theTrack->GetPhi() );
	  beta = beta / pathNorm;
	  beta = beta / 29.979;    // velocity / c
	  
	  // cout << "track = " << itrack + 1 
	  //      << "   beta = " << beta[itrack] << endl;


	}  // condition for tmpDenom	
	else {
	  beta = 0.;
	  betaChisq = -2.;
	} // else condition for tmpDenom
	
	
	// -------------------------------------------------------------------- 
	// -------------------------------------------------------------------- 
	// -------------------------------------------------------------------- 
	// -------------------------------------------------------------------- 
	// -------------------------------------------------------------------- 
	// -------------------------------------------------------------------- 
	// -------------------------------------------------------------------- 
	// -------------------------------------------------------------------- 
	

      }
      else {
	beta = 0.;
	betaChisq = -1;
      }
      
      // ---------------------------------------------------------------------------
      // Date: July 8 2014
      //
      // Right now we do not need this code for beta and chisquare
      //
      if ( fNfpTime != 0 ){
      	fTimeAtFP[itrack] = ( fSumfpTime / fNfpTime ); 
      }
      //
      // ---------------------------------------------------------------------------
      
      
      Double_t fptimesum=0.0;
      Int_t n_fptimesum=0;
      for ( UInt_t ip = 0; ip < fNPlanes; ip++ ){
	if ( fNPlaneTime[ip] != 0 ){
	  fFPTime[ip] = ( fSumPlaneTime[ip] / fNPlaneTime[ip] );
	  fptimesum += fSumPlaneTime[ip];
	  n_fptimesum += fNPlaneTime[ip];
	}
	else{
	  fFPTime[ip] = 1000. * ( ip + 1 );
	}
      }
      Double_t fptime = fptimesum/n_fptimesum;

      // betaChisq[itrack]
      // fFPTime[ind]

      theTrack->SetFPTime(fptime);
      // This can't be right.  Plus if there are no hits, then
      // it is undefined.
      //      theTrack->SetDedx(fdEdX[itrack][0]); // Dedx of first hit
      theTrack->SetBeta(beta);
      theTrack->SetBetaChi2( betaChisq );
      theTrack->SetNPMT(fNPmtHit[itrack]);
      theTrack->SetFPTime( fTimeAtFP[itrack]);

    } // Main loop over tracks ends here.         
   
  } // If condition for at least one track

  return 0;
   
}
//_____________________________________________________________________________
Int_t THcHodoscope::GetScinIndex( Int_t nPlane, Int_t nPaddle ) {
  // GN: Return the index of a scintillator given the plane # and the paddle #
  // This assumes that both planes and 
  // paddles start counting from 0!
  // Result also counts from 0.
  return fNPlanes*nPaddle+nPlane;
}
//_____________________________________________________________________________
Int_t THcHodoscope::GetScinIndex( Int_t nSide, Int_t nPlane, Int_t nPaddle ) {
  return nSide*fMaxHodoScin+fNPlanes*nPaddle+nPlane-1;
}
//_____________________________________________________________________________
Double_t THcHodoscope::GetPathLengthCentral() {
  return fPathLengthCentral;
}
ClassImp(THcHodoscope)
////////////////////////////////////////////////////////////////////////////////
