//*-- Author :    Stephen Wood 30-March-2012

//////////////////////////////////////////////////////////////////////////
//
// THcHitList
//
// Add hitlist to the Hall A detector base
// May not need to inherit from THaDetectorBase since we may end up
// replacing most of the methods
//
//////////////////////////////////////////////////////////////////////////

#include "THcHitList.h"
#include "TError.h"
#include "TClass.h"

using namespace std;

THcHitList::THcHitList()
{
  // Normal constructor.

  fRawHitList = NULL;

}

THcHitList::~THcHitList() {
  // Destructor
}

void THcHitList::InitHitList(THaDetMap* detmap,
				  const char *hitclass, Int_t maxhits) {
  // Probably called by ReadDatabase

  fRawHitList = new TClonesArray(hitclass, maxhits);
  fRawHitClass = fRawHitList->GetClass();
  fNMaxRawHits = maxhits;
  fNRawHits = 0;
  for(Int_t i=0;i<maxhits;i++) {
#if ROOT_VERSION_CODE >= ROOT_VERSION(5,32,0)
    fRawHitList->ConstructedAt(i);
#else
  // The essence of the ConstructedAt code explictly
    TObject* obj = (*fRawHitList)[i];
    R__ASSERT( obj );  // should never happen ...
    if( !obj->TestBit(TObject::kNotDeleted) )
      fRawHitClass->New(obj);
#endif
  }
  
  fdMap = detmap;
}

Int_t THcHitList::DecodeToHitList( const THaEvData& evdata ) {
  THcRawHit* rawhit;
  // cout << " Clearing TClonesArray " << endl;
  fRawHitList->Clear("C");
  fNRawHits = 0;

  for ( Int_t i=0; i < fdMap->GetSize(); i++ ) {
    THaDetMap::Module* d = fdMap->GetModule(i);

    // Loop over all channels that have a hit.
    //    cout << "Crate/Slot: " << d->crate << "/" << d->slot << endl;
    for ( Int_t j=0; j < evdata.GetNumChan( d->crate, d->slot); j++) {
      
      Int_t chan = evdata.GetNextChan( d->crate, d->slot, j );
      if( chan < d->lo || chan > d->hi ) continue;     // Not one of my channels
      
      // Need to convert crate, slot, chan into plane, counter, signal
      // Search hitlist for this plane,counter,signal
      Int_t plane = d->plane;
      Int_t signal = d->signal;
      Int_t counter = d->reverse ? d->first + d->hi - chan : d->first + chan - d->lo;
      //cout << d->crate << " " << d->slot << " " << chan << " " << plane << " "
      // << counter << " " << signal << endl;
      // Search hit list for plane and counter
      // We could do sorting 
      Int_t thishit = 0;
      while(thishit < fNRawHits) {
	rawhit = (THcRawHit*) (*fRawHitList)[thishit];
	if (plane == rawhit->fPlane
	    && counter == rawhit->fCounter) {
	  // cout << "Found as " << thishit << "/" << fNRawHits << endl;
	  break;
	}
	thishit++;
      }

      if(thishit == fNRawHits) {
	rawhit = (THcRawHit*) (*fRawHitList)[thishit];
	fNRawHits++;
	rawhit->fPlane = plane;
	rawhit->fCounter = counter;
      }
	
      // Get the data from this channel
      // Allow for multiple hits
      Int_t nMHits = evdata.GetNumHits(d->crate, d->slot, chan);
      for (Int_t mhit = 0; mhit < nMHits; mhit++) {
	Int_t data = evdata.GetData( d->crate, d->slot, chan, mhit);
	// cout << "Signal " << signal << "=" << data << endl;
	rawhit->SetData(signal,data);
      }
    }
  }
  fRawHitList->Sort(fNRawHits);

  return fNRawHits;		// Does anything care what is returned
}

ClassImp(THcHitList)
