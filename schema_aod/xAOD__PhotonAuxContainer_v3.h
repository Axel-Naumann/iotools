//////////////////////////////////////////////////////////
//   This class has been generated by TFile::MakeProject
//     (Thu Aug 10 16:20:25 2017 by ROOT version 6.10/04)
//      from the StreamerInfo in file AOD.root
//////////////////////////////////////////////////////////


#ifndef xAOD__PhotonAuxContainer_v3_h
#define xAOD__PhotonAuxContainer_v3_h
namespace xAOD {
class PhotonAuxContainer_v3;
} // end of namespace.

#include "Rtypes.h"
#include "xAOD__AuxContainerBase.h"
#include "Riostream.h"
#include <vector>
namespace std {} using namespace std;
#include "ElementLink_DataVector_xAOD__CaloCluster_v1___.h"
namespace std {} using namespace std;
#include "ElementLink_DataVector_xAOD__Vertex_v1___.h"

namespace xAOD {
class PhotonAuxContainer_v3 {

public:
// Nested classes declaration.

public:
// Data Members.
   vector<vector<ElementLink<DataVector<xAOD::CaloCluster_v1> > > > caloClusterLinks;    //
   vector<float>                                                    pt;                  //
   vector<float>                                                    eta;                 //
   vector<float>                                                    phi;                 //
   vector<float>                                                    m;                   //
   vector<unsigned short>                                           author;              //
   vector<unsigned int>                                             OQ;                  //
   vector<float>                                                    f1;                  //
   vector<float>                                                    f3;                  //
   vector<float>                                                    f1core;              //
   vector<float>                                                    f3core;              //
   vector<float>                                                    weta1;               //
   vector<float>                                                    weta2;               //
   vector<float>                                                    fracs1;              //
   vector<float>                                                    wtots1;              //
   vector<float>                                                    e277;                //
   vector<float>                                                    Reta;                //
   vector<float>                                                    Rphi;                //
   vector<float>                                                    Eratio;              //
   vector<float>                                                    Rhad;                //
   vector<float>                                                    Rhad1;               //
   vector<float>                                                    DeltaE;              //
   vector<vector<ElementLink<DataVector<xAOD::Vertex_v1> > > >      vertexLinks;         //
   vector<float>                                                    convMatchDeltaEta1;    //
   vector<float>                                                    convMatchDeltaEta2;    //
   vector<float>                                                    convMatchDeltaPhi1;    //
   vector<float>                                                    convMatchDeltaPhi2;    //

   //vector<ElementLink<DataVector<xAOD::CaloCluster_v1> > > caloClusterLinks;
   /*float                                                    pt;                  //
   float                                                    eta;                 //
   float                                                    phi;                 //
   float                                                    m;                   //
   unsigned short                                           author;              //
   unsigned int                                             OQ;                  //
   float                                                    f1;                  //
   float                                                    f3;                  //
   float                                                    f1core;              //
   float                                                    f3core;              //
   float                                                    weta1;               //
   float                                                    weta2;               //
   float                                                    fracs1;              //
   float                                                    wtots1;              //
   float                                                    e277;                //
   float                                                    Reta;                //
   float                                                    Rphi;                //
   float                                                    Eratio;              //
   float                                                    Rhad;                //
   float                                                    Rhad1;               //
   float                                                    DeltaE;              //
   //vector<ElementLink<DataVector<xAOD::Vertex_v1> > >      vertexLinks;         //
   float                                                    convMatchDeltaEta1;    //
   float                                                    convMatchDeltaEta2;    //
   float                                                    convMatchDeltaPhi1;    //
   float                                                    convMatchDeltaPhi2;

   unsigned char buffer[64000];*/

   /*PhotonAuxContainer_v3();
   PhotonAuxContainer_v3(const PhotonAuxContainer_v3 & );
   virtual ~PhotonAuxContainer_v3();*/

};
} // namespace
#endif