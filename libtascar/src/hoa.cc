#include "hoa.h"
#include <Eigen/QR>
#include <Eigen/SVD>
#include "vbap3d.h"
#include <gsl/gsl_poly.h>

using namespace HOA;

encoder_t::encoder_t()
	: M(0),n_elements(0),leg(NULL)
{
	set_order( 1 );
}

encoder_t::~encoder_t()
{
	if( leg )
		delete [] leg;
}

void encoder_t::set_order(uint32_t order)
{
	M = order;
	if( leg )
		delete [] leg;
	leg = new double[gsl_sf_legendre_array_n(order)];
	n_elements = (order+1)*(order+1);
}


decoder_t::decoder_t()
  : dec(NULL), amb_channels(0), out_channels(0), M(0)
{
}

decoder_t::~decoder_t()
{
  if( dec )
    delete [] dec;
}

template<typename _Matrix_Type_>
_Matrix_Type_ pseudoInverse(const _Matrix_Type_ &a, double epsilon = std::numeric_limits<double>::epsilon())
{
  Eigen::JacobiSVD< _Matrix_Type_ > svd(a ,Eigen::ComputeThinU | Eigen::ComputeThinV);
  double tolerance = epsilon * std::max(a.cols(), a.rows()) *svd.singularValues().array().abs()(0);
  return svd.matrixV() *  (svd.singularValues().array().abs() > tolerance).select(svd.singularValues().array().inverse(), 0).matrix().asDiagonal() * svd.matrixU().adjoint();
}

void decoder_t::create_pinv( uint32_t order, const std::vector<TASCAR::pos_t>& spkpos )
{
  if( dec )
    delete [] dec;
  M = order;
  amb_channels = (M+1)*(M+1);
  out_channels = spkpos.size();
  if( !out_channels )
    throw TASCAR::ErrMsg("Invalid (empty) speaker layout.");
  encoder_t encode;
  encode.set_order(order);
  dec = new float[amb_channels*out_channels];
  Eigen::MatrixXd Bdec(amb_channels,out_channels);
  std::vector<float> B(amb_channels,0);
  for( uint32_t ch=0;ch<out_channels;++ch){
    encode( spkpos[ch].azim(), spkpos[ch].elev(), B );
    for(uint32_t acn=0;acn<amb_channels;++acn)
      Bdec(acn,ch) = B[acn];
  }
  auto Binv(pseudoInverse(Bdec,1e-5));
  float* p_dec(dec);
  for(uint32_t acn=0;acn<amb_channels;++acn)
    for( uint32_t ch=0;ch<out_channels;++ch){
      *p_dec = Binv(ch,acn);
      ++p_dec;
    }
}

void decoder_t::create_allrad( uint32_t order, const std::vector<TASCAR::pos_t>& spkpos )
{
  if( dec )
    delete [] dec;
  M = order;
  amb_channels = (M+1)*(M+1);
  out_channels = spkpos.size();
  if( !out_channels )
    throw TASCAR::ErrMsg("Invalid (empty) speaker layout.");
  dec = new float[amb_channels*out_channels];
  memset(dec,0,sizeof(float)*amb_channels*out_channels);
  std::vector<TASCAR::pos_t> virtual_spkpos(TASCAR::generate_icosahedron());
  while( virtual_spkpos.size() < 3*amb_channels )
    virtual_spkpos = subdivide_and_normalize_mesh( virtual_spkpos, 1 );
  // Computation of an ambisonics decoder matrix Hdecoder_virtual for
  // the virtual array of loudspeakers
  HOA::decoder_t Hdecoder_virtual;
  Hdecoder_virtual.create_pinv( M, virtual_spkpos );
  // computation of VBAP gains for each virtual speaker
  TASCAR::vbap3d_t vbap( spkpos );
  for( uint32_t acn=0;acn<amb_channels;++acn)
      for( uint32_t vspk=0;vspk<virtual_spkpos.size();++vspk ){
        vbap.encode( virtual_spkpos[vspk] );
        for( uint32_t outc=0;outc<out_channels;++outc)
          operator()(acn,outc) += vbap.weights[outc]*Hdecoder_virtual( acn, vspk );
      }
  // calculate normalization:
  HOA::encoder_t enc;
  enc.set_order( order );
  std::vector<float> B(amb_channels,0.0f);
  float tg(0);
  size_t tn(0);
  for( auto it=virtual_spkpos.begin();it!=virtual_spkpos.end();++it){
    enc( it->azim(), it->elev(), B );
    std::vector<float> outsig(out_channels,0.0f);
    operator()(B,outsig);
    float g(0);
    for( uint32_t ch=0;ch<out_channels;++ch)
      g += outsig[ch];
    tg+= g;
    ++tn;
  }
  tg/=tn;
  tg = 1/tg;
  for( uint32_t k=0;k<amb_channels*out_channels;++k)
    dec[k] *= tg;
  //DEBUG(tg);
  //DEBUG(20*log10(tg));
}


/*
 * This function is taken from Ambisonics Decoder Toolbox by A. Heller
 * (https://bitbucket.org/ambidecodertoolbox/adt/src/master/)
 * GNU AFFERO GENERAL PUBLIC LICENSE Version 3
 *
 * For License see file LICENSE.hoa.cc.legendre_poly
 */
// LegendrePoly.m by David Terr, Raytheon, 5-10-04
//
// Given nonnegative integer n, compute the 
// Legendre polynomial P_n. Return the result as a vector whose mth
// element is the coefficient of x^(n+1-m).
// polyval(LegendrePoly(n),x) evaluates P_n(x).
std::vector<double> HOA::legendre_poly( size_t n )
{
  if( n==0 )
    return { 1.0 };
  if( n==1 )
    return { 1.0, 0.0 };
  std::vector<double> pkm2(n+1,0.0);
  pkm2[n] = 1.0;
  std::vector<double> pkm1(n+1,0.0);
  pkm1[n-1] = 1.0;
  std::vector<double> pk(n+1,0.0);
  for( size_t k=2;k<=n;++k){
    pk = std::vector<double>(n+1,0.0);
    for( size_t e=n-k+1;e<=n;e+=2)
      pk[e-1] = (2*k-1)*pkm1[e] + (1.0-(double)k)*pkm2[e-1];
    pk[n] = pk[n] + (1.0-(double)k)*pkm2[n];
    for( auto it=pk.begin();it!=pk.end();++it)
      *it /= k;
    if( k<n ){
      pkm2 = pkm1;
      pkm1 = pk;
    }
  }
  return pk;
}

std::vector<double> HOA::roots( const std::vector<double>& P1 )
{
  std::vector<double> P(P1.size(),0.0);
  for( size_t k=0;k<P.size();++k)
    P[k] = P1[P.size()-k-1];
  while( (P.size() > 0) && (P[P.size()-1]==0) )
    P.erase(P.end()-1);
  if( P.size() < 1u )
    return {};
  if( P.size() < 2u )
    return {};
  std::vector<double> z(2*(P.size()-1),0.0);
  gsl_poly_complex_workspace* ws(gsl_poly_complex_workspace_alloc(P.size()));
  gsl_poly_complex_solve(P.data(), P.size(), ws, z.data());
  // return only real parts:
  std::vector<double> z2(P.size()-1,0.0);
  for(uint32_t k=0;k<z2.size();++k){
    z2[k] = z[2*k];
  }
  gsl_poly_complex_workspace_free (ws);
  z = z2;
  std::sort(z.begin(),z.end());
  return z;
}

std::vector<double> HOA::maxre_gm( size_t M )
{
  // See also dissertation J. Daniel (2001), page 184.
  std::vector<double> Z(HOA::roots(HOA::legendre_poly( M+1 )));
  double rE(*(std::max_element(Z.begin(),Z.end())));
  std::vector<double> gm(M+1,1.0);
  for( size_t m=1;m<=M;++m)
    gm[m] = gsl_sf_legendre_Pl(m,rE);
  return gm;
}

size_t factorial( size_t n )
{
  size_t fact(1);
  for(size_t c=1; c<=n; ++c)
    fact = fact * c;
  return fact;
}

std::vector<double> HOA::inphase_gm( size_t M )
{
  // See also dissertation J. Daniel (2001), page 184.
  std::vector<double> gm(M+1,1.0);
  for( size_t m=1;m<=M;++m)
    gm[m] = (double)(factorial(M)*factorial(M+1))/(double)(factorial(M+m+1)*factorial(M-m));
  return gm;
}

void decoder_t::modify( const modifier_t& m )
{
  std::vector<double> gm( M+1, 1.0 );
  switch( m ){
  case basic :
    break;
  case maxre :
    gm = HOA::maxre_gm( M );
    break;
  case inphase :
    gm = HOA::inphase_gm( M );
    break;
  }
  size_t acn(0);
  for( int m=0;m<=M;++m )
    for( int l=-m;l<=m;++l ){
      for( uint32_t ch=0;ch<out_channels;++ch )
        operator()(acn,ch) *= gm[m];
      ++acn;
    }
}


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
