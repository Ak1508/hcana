#ifndef ROOT_THcRawHit
#define ROOT_THcRawHit

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// THcRawHit                                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#include "TObject.h"

class THcRawHit : public TObject {

public:
  THcRawHit(Int_t plane=0, Int_t counter=0) :
    fPlane(plane), fCounter(counter) {};
 THcRawHit( const THcRawHit& rhs ) : TObject(rhs) {}
  THcRawHit& operator=( const THcRawHit& rhs )
    { TObject::operator=(rhs);
      if (this != &rhs) { fPlane = rhs.fPlane; fCounter = rhs.fCounter; }
      return *this; };

  virtual ~THcRawHit() {}

  // This line causes problem
  //  virtual void Clear( Option_t* opt="" )=0;

  //  virtual Bool_t  operator==( const THcRawHit& ) = 0;
  //  virtual Bool_t  operator!=( const THcRawHit& ) = 0;

  virtual void SetData(Int_t signal, Int_t data) {};
  virtual Int_t GetData(Int_t signal) {return 0;}; /* Ref time subtracted */
  virtual Int_t GetRawData(Int_t signal) {return 0;} /* Ref time not subtracted */
  virtual void SetReference(Int_t signal, Int_t reference) {};
  virtual Bool_t HasReference(Int_t signal) {return kFALSE;};
  virtual Int_t GetReference(Int_t signal) {return 0;};

  // Derived objects must be sortable and supply Compare method
  //  virtual Bool_t  IsSortable () const {return kFALSE; }
  //  virtual Int_t   Compare(const TObject* obj) const {return 0;}
  virtual Bool_t  IsSortable () const {return kTRUE; }
  virtual Int_t   Compare(const TObject* obj) const;

  Int_t fPlane;
  Int_t fCounter;

 private:

  ClassDef(THcRawHit,0)      // Raw Hit Base Class
};

#endif
