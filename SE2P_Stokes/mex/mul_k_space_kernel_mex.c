#include "mex.h"
#include "math.h"

#define G1   prhs[0]
#define G2   prhs[1]
#define G3   prhs[2]
#define OPT  prhs[3] 
#define XI   prhs[4] 
#define ETA  prhs[5]

#define H1   plhs[0]  // Output
#define H2   plhs[1]  // Output
#define H3   plhs[2]  // Output

#define PI 3.141592653589793
#define NDIMS 3

// select k-space scaling tensor
#ifdef HASIMOTO
#include "hasimoto_op_fd.h"
#elif BEENAKKER
#include "beenakker_op_fd.h"
#else
#error "Must provide -D<method> to compiler"
#endif

#ifndef VERBOSE
#define VERBOSE 0
#endif

static inline int is_odd(int p)
{
    return p&1;
}

int k_vec(double* k, int M, double L, int spf)
{
    const int Mq = M*spf;
    const double c = 2*PI/(L*spf);
    int idxz;
    if(is_odd(Mq))
    {
	for(int i=-(Mq-1)/2, j=0; i<=(Mq-1)/2; i++, j++)
	    k[j]=c*i;
	idxz = (Mq-1)/2;
    }
    else
    {
	for(int i=-Mq/2, j=0; i<=Mq/2-1; i++, j++)
	    k[j]=c*i;
	idxz = Mq/2;
    }
    return idxz;
}

void mexFunction(int nlhs,       mxArray *plhs[],
		 int nrhs, const mxArray *prhs[] )
{
    if(VERBOSE)
	mexPrintf("Stokes Ewald FFT k-scaling: %s\n", OP_TAG);

    // dimensions
    const mwSize *dims = mxGetDimensions(G1); // assume all G have same dims

    // real part of input arrays
    const double* restrict g1r = mxGetPr(G1);
    const double* restrict g2r = mxGetPr(G2);
    const double* restrict g3r = mxGetPr(G3);

    // imaginary parts
    const double* restrict g1i = mxGetPi(G1);
    const double* restrict g2i = mxGetPi(G2);
    const double* restrict g3i = mxGetPi(G3);

    // output arrays, complex
    H1 = mxCreateNumericArray(NDIMS,dims,mxDOUBLE_CLASS,mxCOMPLEX);
    H2 = mxCreateNumericArray(NDIMS,dims,mxDOUBLE_CLASS,mxCOMPLEX);
    H3 = mxCreateNumericArray(NDIMS,dims,mxDOUBLE_CLASS,mxCOMPLEX);

    double* h1r = mxGetPr(H1);
    double* h2r = mxGetPr(H2);
    double* h3r = mxGetPr(H3);

    double* h1i = mxGetPi(H1);
    double* h2i = mxGetPi(H2);
    double* h3i = mxGetPi(H3);

    // parameters (will core dump i field not present in struct OPT)
    const int M = (int) mxGetScalar(mxGetField(OPT,0,"M"));
    const int Mz = (int) mxGetScalar(mxGetField(OPT,0,"Mz"));
    const int spf = (int)  mxGetScalar(mxGetField(OPT,0,"sampling_factor"));
    const double L = mxGetScalar(mxGetField(OPT,0,"L"));
    const double Lz = mxGetScalar(mxGetField(OPT,0,"Lz"));

    const double xi = mxGetScalar(XI);
    const double eta = mxGetScalar(ETA);
    const double c = (1-eta)/(4*xi*xi);

    // allocate k-vectors
    double* restrict kx=mxMalloc(dims[0]*sizeof(double));
    double* restrict ky=mxMalloc(dims[1]*sizeof(double));
    double* restrict kappa=mxMalloc(dims[2]*sizeof(double));

    // evaluate k-vectors
    int idxz_x = k_vec(kx, M, L, 1);
    int idxz_y = k_vec(ky, M, L, 1);
    k_vec(kappa, Mz, Lz, spf);

    // scrach vars
    double B[3][3];
    double G[3];
    double k[3];
    int idx;
    double q;

    for(int i2 = 0; i2<dims[2]; i2++)
    {
    	for(int i1 = 0; i1<dims[1]; i1++)
    	{
    	    for(int i0 = 0; i0<dims[0]; i0++)
    	    {
    		idx = i0 + i1*dims[0] + i2*dims[0]*dims[1]; // FORTRAN index
		k[0] = kx[i0]; 
		k[1] = ky[i1]; 
		k[2] = kappa[i2];		
    		if( i0!=idxz_x || i1!=idxz_y)       // exclude k=0 
    		{                             
    		    q = exp(-c*( k[0]*k[0] + k[1]*k[1] + k[2]*k[2] ));
    		    op_BB(B,k,xi);

    		    // real part
    		    G[0] = g1r[idx];
    		    G[1] = g2r[idx];
    		    G[2] = g3r[idx];

    		    h1r[idx] = q*( B[0][0]*G[0]+B[0][1]*G[1]+B[0][2]*G[2] );
    		    h2r[idx] = q*( B[1][0]*G[0]+B[1][1]*G[1]+B[1][2]*G[2] );
    		    h3r[idx] = q*( B[2][0]*G[0]+B[2][1]*G[1]+B[2][2]*G[2] );

    		    // imaginary part
    		    G[0] = g1i[idx];
    		    G[1] = g2i[idx];
    		    G[2] = g3i[idx];

    		    h1i[idx] = q*( B[0][0]*G[0]+B[0][1]*G[1]+B[0][2]*G[2] );
    		    h2i[idx] = q*( B[1][0]*G[0]+B[1][1]*G[1]+B[1][2]*G[2] );
    		    h3i[idx] = q*( B[2][0]*G[0]+B[2][1]*G[1]+B[2][2]*G[2] );

    		}
    		else // don't assume initalized to zero
    		{
    		    h1r[idx] = 0; h2r[idx] = 0; h3r[idx] = 0;
    		    h1i[idx] = 0; h2i[idx] = 0; h3i[idx] = 0;
    		}
    	    }
    	}
    }

    mxFree(kx);
    mxFree(ky);
    mxFree(kappa);
}
