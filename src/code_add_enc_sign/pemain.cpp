/*
   Boneh & Franklin's Identity Based Encryption
   
   Set-up phase

   After this program has run the file common.ibe contains

   <Size of prime modulus in bits>
   <Prime p>
   <Prime q (divides p+1) >
   <Point P - x coordinate>
   <Point P - y coordinate>
   <Point Ppub - x coordinate>
   <Point Ppub - y coordinate>
   <Cube root of unity in Fp2 - x component >
   <Cube root of unity in Fp2 - y component >

   The file master.ibe contains

   <The master secret s>

   Requires: zzn2.cpp big.cpp zzn.cpp ecn.cpp

 */
#include <time.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <thread>
using namespace std;

#include "ecn.h"
#include "zzn.h"
#include "zzn2.h"

//C++���������߱������ⲿ�ִ�����C���Ա�д�ģ����������ʱӦ��ʹ��C���Ե�����Լ��
extern "C"
{
   #include"miracl.h"
   #include"mirdef.h"
}

//2015�����
//�ṩһ�������Բ�,Ϊ���ڲ�ͬ�汾��MSVC����ʱ��֮���ṩһ�ּ����Ի��ƣ�ʹ�þɴ����ܹ����°汾�Ŀ����������У�������Ҫ�Դ�����д����޸ġ�
FILE* __cdecl __iob_func(unsigned i) {
    return __acrt_iob_func(i);
}

#ifdef __cplusplus
extern "C" 
#endif
//extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }
FILE _iob[3] = {__iob_func(0)[0], __iob_func(1)[1], __iob_func(2)[2]}; 

//һ���ض���Microsoft Visual C++��������ָ����ڿ�������������Ϊ�����������Ǹ��������������ӹ����в�ҪĬ�����ӵ�libc.lib����⡣
#pragma comment(linker, "/NODEFAULTLIB:libc.lib")


#define renum 2500

#define HASH_LEN 32
#define HASH_LEN1 20   //������H2����Ϊ��������q��160λ�Ķ�����������160/8=20
                                        //H2�в���sha256Ҫ��HASH_LEN1 ������32�ı�������ˣ��Լ���H2�ڲ��������Ϊsha-1


#define PBITS 512
#define QBITS 160

// Using SHA-256 as basic hash algorithm

//
// Define one or the other of these
//
// Which is faster depends on the I/M ratio - See imratio.c
// Roughly if I/M ratio > 16 use PROJECTIVE, otherwise use AFFINE
//

// #define AFFINE
#define PROJECTIVE

// Define this to use this idea ftp://ftp.computing.dcu.ie/pub/resources/crypto/short.pdf
// which enables denominator elimination
#define SCOTT

Miracl precision(16,0);  // increase if PBITS increases. (32,0) for 1024 bit p

/*----------------------------------------------------------------------------Tate Paring ��������Ҫ�ĺ���-----------------------------------------------------*/
void extract(ECn& A,ZZn& x,ZZn& y)  //��������
{ 
    x=(A.get_point())->X;
    y=(A.get_point())->Y;
}

void extract(ECn& A,ZZn& x,ZZn& y,ZZn& z)  //��Ӱ����
{ 
    big t;
    x=(A.get_point())->X;
    y=(A.get_point())->Y;
    t=(A.get_point())->Z;
    if (A.get_status()!=MR_EPOINT_GENERAL) 
        z=1;
    else                                   
        z=t;
}

//
// Line from A to destination C. Let A=(x,y)
// Line Y-slope.X-c=0, through A, so intercept c=y-slope.x
// Line Y-slope.X-y+slope.x = (Y-y)-slope.(X-x) = 0
// Now evaluate at Q -> return (Qy-y)-slope.(Qx-x)
//

ZZn2 line(ECn& A, ECn& C, ZZn& slope, ZZn2& Qx, ZZn2& Qy)  //������Բ�����ϵĵ�Q��A��C��ֱ��
{ 
    ZZn2 n=Qx,w=Qy;
    ZZn x,y,z,t;
#ifdef AFFINE
    extract(A,x,y);
    n-=x; n*=slope;            // 2 ZZn muls
    w-=y; n-=w;
#endif
#ifdef PROJECTIVE
    extract(A,x,y,z);
    x*=z; t=z; z*=z; z*=t;          
    n*=z; n-=x;                // 9 ZZn muls
    w*=z; w-=y; 
    extract(C,x,y,z);
    w*=z; n*=slope; n-=w;                     
#endif
    return n;
}

#ifndef SCOTT

//
// Vertical line through point A
//

ZZn2 vertical(ECn& A,ZZn2& Qx)
{
    ZZn2 n=Qx;
    ZZn x,y,z;
#ifdef AFFINE
    extract(A,x,y);
    n-=x;
#endif
#ifdef PROJECTIVE
    extract(A,x,y,z);
    z*=z;                    
    n*=z; n-=x;                // 3 ZZn muls
#endif
    return n;
}

#endif

//
// Add A=A+B  (or A=A+A) 
// Bump up num and denom
//
// AFFINE doubling     - 12 ZZn muls, plus 1 inversion
// AFFINE adding       - 11 ZZn muls, plus 1 inversion
//
// PROJECTIVE doubling - 26 ZZn muls
// PROJECTIVE adding   - 34 ZZn muls
//


void g(ECn& A,ECn& B,ZZn2& Qx,ZZn2& Qy,ZZn2& num) 
{
    ZZn  lam,mQy;
    ZZn2 d,u;
    big ptr;
    ECn P=A;

// Evaluate line from A
    ptr=A.add(B);

#ifndef SCOTT
    if (A.iszero())   { u=vertical(P,Qx); d=1; }
    else
    {
#endif
        if (ptr==NULL)
            u=1;
        else 
        {
            lam=ptr;
            u=line(P,A,lam,Qx,Qy);
        }
#ifndef SCOTT
        d=vertical(A,Qx);
    }

    num*=(u*conj(d));    // 6 ZZn muls  
#else
// denominator elimination!
    num*=u;
#endif
}

//
// Tate Pairing 
//

BOOL fast_tate_pairing(ECn& P, ZZn2& Qx, ZZn2& Qy, Big& q, ZZn2& res) //P:����Ԫ��Qx,Qy:��Բ�����ϵĵ�Q�����꣬q:�����ף�res:˫���ԶԽ��
{ 
    int i,nb;
    Big n,p;
    ECn A;


// q.P = 2^17*(2^142.P +P) + P

    res=1;
    A=P;    // reset A

#ifdef SCOTT
// we can avoid last iteration..
    n=q-1;
#else
    n=q;
#endif
    nb=bits(n);

    for (i=nb-2;i>=0;i--)
    {
        res*=res;         
        g(A,A,Qx,Qy,res); 
        if (bit(n,i))
            g(A,P,Qx,Qy,res);       
    }

#ifdef SCOTT
    if (A!=-P || res.iszero()) return FALSE;
#else
    if (!A.iszero()) return FALSE;
#endif

    p=get_modulus();         // get p
    res= pow(res,(p+1)/q);   // raise to power of (p^2-1)/q
    res=conj(res)/res;
    if (res.isunity()) return FALSE;
    return TRUE;   
}
BOOL ecap(ECn& P,ECn& Q,Big& order,ZZn2& cube,ZZn2& res)  //P:����Ԫ��Q:����㣬order:�����ף�cube:��������res:˫���ԶԽ��
{
     ZZn2 Qx,Qy;
     Big xx,yy;
#ifdef SCOTT
     ZZn a,b,x,y,ib,w,t1,y2,ib2;
#else
     ZZn2 lambda,ox;
#endif
     Q.get(xx,yy);
     Qx=(ZZn)xx*cube;
     Qy=(ZZn)yy;

#ifndef SCOTT
// point doubling
     lambda=(3*Qx*Qx)/(Qy+Qy);
     ox=Qx;
     Qx=lambda*lambda-(Qx+Qx);
     Qy=lambda*(ox-Qx)-Qy;
#else
 //explicit point subtraction
     Qx.get(a,b);
     y=yy;
     ib=(ZZn)1/b;

     t1=a*b*b;
     y2=y*y;
     ib2=ib*ib;
     w=y2+2*t1;
     x=-w*ib2;
     y=-y*(w+t1)*(ib2*ib);
     Qx.set(x); 
     Qy.set((ZZn)0,y);

#endif

     if (fast_tate_pairing(P,Qx,Qy,order,res)) return TRUE;
     return FALSE;
}


//
// ecap(.) function - apply distortion map
//
// Qx is in ZZn if SCOTT is defined. Qy is in ZZn if SCOTT is not defined. 
// This can be exploited for some further optimisations. 
/*----------------------------------------------------------------------------Tate Paring ��������Ҫ�ĺ���-----------------------------------------------------*/


/*----------------------------------------------------------------------------���Hash��������ĺ���-----------------------------------------------------*/
// ʵ����һ�����ַ�����ϣ��С��ģ�� p �Ĵ����� Big �ĺ��� H1
Big H1(char *string)
{ // Hash a zero-terminated string to a number < modulus
    Big h,p;
    char s[HASH_LEN];
    int i,j; 
    sha256 sh;

    shs256_init(&sh);

    for (i=0;;i++)
    {
        if (string[i]==0) 
            break;
        shs256_process(&sh,string[i]);
    }
    shs256_hash(&sh,s);
    p=get_modulus();
	//cout<<"modulus"<<p<<endl;//�Լ��ӵĲ鿴pֵ����䣬ͨ��pֵ��֪get_modulus()������get_mip()������
	//��get_mip()�õ����ǵ�ǰ��������Ⱥ�Ľ�ֵq.
    h=1; j=0; i=1;
    forever
    {
        h*=256; 
        if (j==HASH_LEN)  
        {h+=i++; j=0;}
        else        
            h+=s[j++];
        if (h>=p)
            break;
    }
    h%=p;
    return h;
}

//��δ���ʵ����һ���� Fp2 Ԫ�ع�ϣ��һ�� n �ֽ��ַ����ĺ��� H2�����������Ŀ���ǽ� Fp2 ���е�Ԫ��ӳ�䵽һ���̶����ȵ��ַ���
//�����������Ҫ�����ǽ� Fp2 ���е�Ԫ��ͨ�� SHA-1 ��ϣ�㷨ת��Ϊһ���̶����ȵ��ַ���
int H2(ZZn2 x,char *s)
{ // Hash an Fp2 to an n-byte string s[.]. Return n
    sha sh;
    Big a,b;
    int m;  

    shs_init(&sh);
    x.get(a,b);

    while (a>0)
    {
        m=a%160;
        shs_process(&sh,m);
        a/=160;
    }
    while (b>0)
    {
        m=b%160;
        shs_process(&sh,m);
        b/=160;
    }
    shs_hash(&sh,s);

  return HASH_LEN1;
	/*sha256 sh;
    Big a,b;
    int m;

    shs256_init(&sh);
    x.get(a,b);

    while (a>0)
    {
        m=a%256;
        shs256_process(&sh,m);
        a/=256;
    }
    while (b>0)
    {
        m=b%256;
        shs256_process(&sh,m);
        b/=256;
    }
    shs256_hash(&sh,s);

  return HASH_LEN1;
	// return 20;*/
}

// ��δ���ʵ����һ��������ֹ�ַ�����ϣ��С�ڸ���ģ�� qm �Ĵ����� Big �ĺ��� H3��
// �����������Ҫ�����ǽ�������ַ���ͨ�� SHA-1 ��ϣ�㷨ת��Ϊһ���̶����ȵĹ�ϣֵ��
// Ȼ�������ϣֵӳ�䵽һ������������ȷ�����������С�ڸ�����ģ�� qm
Big H3(char *string,Big qm)
{ // Hash a zero-terminated string to a number < modulus q
    Big h;
    char s[HASH_LEN1];
    int i,j; 
    sha sh;

    shs_init(&sh);

    for (i=0;;i++)
    {
        if (string[i]==0) break;
        shs_process(&sh,string[i]);
    }
    shs_hash(&sh,s);
    //q=get_modulus();
	//cout<<"modulus"<<p<<endl;//�Լ��ӵĲ鿴pֵ����䣬ͨ��pֵ��֪get_modulus()�õ�����Բ�������������������P
    h=1; j=0; i=1;
    forever
    {
        h*=160; 
        if (j==HASH_LEN1)  
        {h+=i++; j=0;}
        else        
            h+=s[j++];
        if (h>=qm) break;
    }
    h%=qm;
    return h;
}
   
//
// Given y, get x=(y^2-1)^(1/3) mod p (from curve equation)
// �ڸ��� (y) ֵ������£�
// �ҵ�������Բ���߷��� (y^2 = x^3 + 1) �� (x) ֵ
//

Big getx(Big y)
{
    Big p=get_modulus();
    Big t=modmult(y+1,y-1,p);   // avoids overflow
    return pow(t,(2*p-1)/3,p);
}
 
// MapToPoint
//��һ���ַ�����ʶ����ID��ӳ�䵽��Բ�����ϵ�һ����Ĺ���
ECn map_to_point(char *ID)
{
    ECn Q;
    Big x0,y0=H1(ID);
 
    x0=getx(y0);

    Q.set(x0,y0);

    return Q;
}




/*-------------------------------------------- Sign + Enc --------------------------------------------*/

// �û���Կ�ṹ��(Enc+sign)
typedef struct UserKey {
    
    ECn d1, d2; //�û�����˽Կ
    ECn Qid;    //�û����ܹ�Կ

    Big z;      //�û�ǩ��˽Կ

	ECn g, g_, Z, h, h1, g1;     //�û�ǩ����Կ
    ZZn2 e_h_g_;
    
}UserKey;

// ʱ�����Žṹ��
typedef struct TimetrapDoor {
    ECn st1;
    ECn st2;
}Timetrapdoor;

// Enc��������
typedef struct params {
    ECn P, P1, P2, P3, P4;
    ECn u[257];
    ECn v[257];
    ZZn2 e_g1_g2;
    ZZn2 e_g3_g4;
}params;

// PKG˽Կ
typedef struct sk_pkg {
    Big alpha;
    ECn sk_pkg;
}sk_pkg;

// TimeServer˽Կ
typedef struct sk_ts {
    Big beta;
    ECn sk_ts;
}sk_ts;

typedef struct Ciphtertext {

    ZZn2 c1;              // ����������Ϣ������
    ECn c2, c3, c4, c5;

	ZZn2 c6;             // ����ǩ��������
    ECn c7;

}Ciphtertext;

typedef struct Ciphtertext1 {

    ECn c1;              // ����
    ZZn2 c2;
    ECn V;              // ǩ��

}Ciphtertext1;



// ǩ���ṹ��
typedef struct
{
    ECn X;
	Big s;
    ZZn2 es;
}SignOfMessage;

/*------------------------------------�Զ��庯��--------------------------------------------*/
void SkGen(params&params, sk_pkg& sk_pkg, Big q,UserKey& userkey, ZZn2 cube) {
    

    // ���ɽ���˽Կ
    Big r1 = rand(q);
    userkey.d1 = sk_pkg.sk_pkg + ( r1 * userkey.Qid );
    userkey.d2 = r1 * params.P;

    // ����ǩ��˽Կ
	userkey.z = rand(q);

    //����es
	ecap(userkey.h, userkey.g_, q, cube, userkey.e_h_g_ );
	if (ecap(userkey.h, userkey.g_, q, cube, userkey.e_h_g_)) {
		cout << "es_e_h_g_ success" << endl;
	}
	else {
		cout << "es_e_h_g_ error" << endl;
	}

}

void StGen(params& params, sk_ts& sk_ts, Big q, ECn& T, Timetrapdoor& st) {
    Big r4 = rand(q);
    st.st1 = sk_ts.sk_ts + ( r4 * T) ;
    st.st2 = r4 * params.P;
}

void Enc(ZZn2& g0, Big& m, params& params, ECn& QT, Big q, Ciphtertext& CT, SignOfMessage& sign, UserKey& userkey) {
    Big r2;
    Big r3;
    r2 = rand(q);
    r3 = rand(q);

    ZZn2 Q;
    Q = pow(g0, m);

    CT.c1 = Q *  pow(params.e_g1_g2, r2) * pow(params.e_g3_g4, r3);

    CT.c2 = r2 * params.P;

    CT.c3 = r2 * userkey.Qid;

    CT.c4 = r3 * params.P;

    CT.c5 = r3 * QT;

	CT.c6 = sign.es * pow(params.e_g1_g2, r2) * pow(params.e_g3_g4, r3);

	CT.c7 = sign.X;
}

void Dec(Ciphtertext& CT, UserKey& privatekey, TimetrapDoor& st, Big& mdec, ZZn2& cube, ZZn2& g0, Big q, SignOfMessage& m_sign) {
	

    ZZn2 e1, e2, e3, e4, temp, Q;
    
    ecap(privatekey.d2, CT.c3, q, cube, e1);
    
	if (ecap(privatekey.d2, CT.c3, q, cube, e1)) {
		cout << "e1 success" << endl;
	}
	else {
		cout << "e1 error" << endl;
	}

    ecap(st.st2, CT.c5, q, cube ,e2);
    if (ecap(st.st2, CT.c5, q, cube, e2)) {
        cout << "e2 success" << endl;
    }
    else {
        cout << "e2 error" << endl;
    }


    ecap(CT.c2, privatekey.d1, q, cube, e3);
    if (ecap(CT.c2, privatekey.d1, q, cube, e3)) {
        cout << "e3 success" << endl;
    }
    else {
        cout << "e3 error" << endl;
    }

    ecap(CT.c4, st.st1, q, cube , e4);
    if (ecap(CT.c4, st.st1, q, cube, e4)) {
        cout << "e4 success" << endl;
    }
    else {
        cout << "e4 error" << endl;
    }

    temp = CT.c1;
	temp *= e1;
	temp *= e2;
	temp /= e3;
	temp /= e4;

	for (Big i = 1; i <= 99999; i += 1) {
		Q = pow(g0, i);
		if (Q == temp)  {
            mdec = i; 
            break;
		}
    }

    temp = CT.c6;
    temp *= e1;
    temp *= e2;
    temp /= e3;
    temp /= e4;

    m_sign.es = temp;

    m_sign.X = CT.c7;
	
}
/*
void DecMulthread(Ciphtertext CT, UserKey privatekey, TimetrapDoor st, Big& mdec, ZZn2 cube, ZZn2 g0, Big q, SignOfMessage& m_sign) {


    ZZn2 e1, e2, e3, e4, temp, Q;

	thread t1(ecap, privatekey.d2, CT.c3, q, cube, e1);

	thread t2(ecap, st.st2, CT.c5, q, cube, e2);

	thread t3(ecap, CT.c2, privatekey.d1, q, cube, e3);

	thread t4(ecap, CT.c4, st.st1, q, cube, e4);

    t1.join();
    t2.join();
	t3.join();
	t4.join();

    temp = CT.c1;
    temp *= e1;
    temp *= e2;
    temp /= e3;
    temp /= e4;

    for (Big i = 1; i <= 99999; i += 1) {
        Q = pow(g0, i);
        if (Q == temp) {
            mdec = i;
            break;
        }
    }

    temp = CT.c6;
    temp *= e1;
    temp *= e2;
    temp /= e3;
    temp /= e4;

    m_sign.es = temp;

    m_sign.X = CT.c7;
}
*/




// ǩ������
void sign(ZZn2 cube, Big q, Big fid, UserKey& userkey, SignOfMessage& SignOfMessage, Big& m, Big p)
{
    /*
    ZZn2 ecap1;
    ecap(userkey.g_, userkey.h, q, cube, ecap1);
    if (ecap(userkey.g_, userkey.h, q, cube, ecap1)) {
        cout << "ecap(g',h) success" << endl;
    }
    else
    {
        cout << "ecap(g',h) failed" << endl;
    }
    */

    SignOfMessage.s = rand(q);
    SignOfMessage.X = (inverse((userkey.z + fid), q)) * ((SignOfMessage.s * userkey.h) + (m * userkey.h1) + (m * userkey.g1));
    SignOfMessage.es = pow(userkey.e_h_g_, SignOfMessage.s);

}



// ��֤����


BOOL verify(Big fid, UserKey& userkey, SignOfMessage& sign, ZZn2 cube, Big q, Big m)
{
    ZZn2 ecap1, ecap2;
    ecap((userkey.Z + (fid * userkey.g_)), sign.X, q, cube, ecap1);
    if (ecap((userkey.Z + (fid * userkey.g_)), sign.X, q, cube, ecap1)) {
        cout << "502:ecap_left success" << endl;
    }
    else
    {
        cout << "502:ecap_left error" << endl;
    }


    ecap((m * userkey.h1) + (m * userkey.g1), userkey.g_, q, cube, ecap2);
    if (ecap((m * userkey.h1) + (m * userkey.g1), userkey.g_, q, cube, ecap2)) {
        cout << "513:ecap_right success" << endl;
    }
    else
    {
        cout << "513:ecap_right error" << endl;
    }


    if (ecap1 == (ecap2 * sign.es)) {
        return true;
    }
    else
    {
        return false;
    }
}

// ̬ͬ����
void EvalSignEnc(Ciphtertext& CT1, Ciphtertext& CT2, Ciphtertext& CT3) {

	CT3.c1 = CT1.c1 * CT2.c1;
	CT3.c2 = CT1.c2 + CT2.c2;
	CT3.c3 = CT1.c3 + CT2.c3;
	CT3.c4 = CT1.c4 + CT2.c4;
	CT3.c5 = CT1.c5 + CT2.c5;
	CT3.c6 = CT1.c6 * CT2.c6;
	CT3.c7 = CT1.c7 + CT2.c7;
}






/*---------------------------------------------------------���Hash��������ĺ���-----------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------------------������---------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------------------������---------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------------------������---------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------------------������---------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------------------������---------------------------------------------------------------------------------------------------*/
int main()
{

    //ofstream common("common.ibe");
    //ofstream master("master.ibe");
    // �Զ����Ʒ�ʽ���ļ�  
    ofstream outputFile1("Ciphertext.ibe");
    ofstream outputFile3("Ciphertext2.ibe");
    ofstream outputFile2("St.ibe");

    // ����ļ��Ƿ�ɹ���  
    if (!outputFile1) {
        std::cerr << "�޷����ļ�!" << std::endl;
        return 1; // ���ش������  
    }
    // ����ļ��Ƿ�ɹ���  
    if (!outputFile2) {
        std::cerr << "�޷����ļ�!" << std::endl;
        return 1; // ���ش������  
    }


    ECn P, Q, R, Ppub, Qid;                    //Ppub:Master Public Key, Qid:Identity Public Key P:����Ԫ��Q:����㣬R:�м����
    ZZn2 Qx, Qy, gid, gid1, cube, w;//�Լ�     //gid:˫���ԶԽ����gid1:˫���ԶԽ����cube:��������w:�������� Qx,Qy:��Բ�����ϵĵ�Q������
    Big px, py, qx, qy, ab, r1;//�Լ�         //px,py:��Բ�����ϵĵ�P�����꣬qx,qy:��Բ�����ϵĵ�Q�����꣬ab:��Բ�����ϵĵ�Qid�����꣬r1:��ϣֵ
    int i, tag;//�Լ� 

    Big a, b, c, d1, d2, p, q, t, n, cof, x, y;  //a:˽Կ��b:˽Կ��c:˽Կ��d1:˽Կ��d2:˽Կ��p:����p��q:����q��t:�����ף�n:�������cof:ϵ����x:��Բ�����ϵĵ�����꣬y:��Բ�����ϵĵ������
    big x1, y1, x2, y2;                          //x1,y1:��Բ�����ϵĵ�����꣬x2,y2:��Բ�����ϵĵ������
    long seed;                                   //seed:���������
    char pad[HASH_LEN1];//�Լ�                   //pad:��ϣֵ
    //char pad[20]={0};
    clock_t start_time, end_time;
    float t_dec;
    ZZn2 g0;
	Big m1, m2, m3;                             	//m1:���ģ�m2:���ģ�m3:����
    params params;
	ECn QtTimeToBeDec, QtTimeNow;
    Big fid;
    SignOfMessage m1_sign, m2_sign, m3_sign;
    UserKey key_Alice, key_Bob;                   //�����û�Alice Bob��Կ   
    sk_pkg sk_pkg;
    sk_ts sk_ts;
    TimetrapDoor st_TimeNow;
    Ciphtertext CT1, CT2, CT3;
    Ciphtertext1 CT1111;

    /*------------------------------------------------Enc+Sign��������---------------------------------------------------------------*/
    char Alice[100] = "Alice@email.com";
    char Bob[100] = "Bob@@email.com";
    char TimeToBeDec[9] = "20241212";
    char TimeNow[9] = "20241212";



    miracl* mip = &precision;                    //miracl* mip:����

    cout << "������Щ����������ʱ����1���룬���л������������ظ�ִ��" << renum << "�� " << endl;
    cout << "Enter 9 digit random number seed  = ";
    cin >> seed;
    irand(seed);

    // SET-UP
    /*-------------------------------------------------------------����������q-------------------------------------------------------------------------*/
    q = pow((Big)2, 159) + pow((Big)2, 17) + 1; 

    cout << "q= " << q << endl;

/*--------------------------------------------------------------��������p-------------------------------------------------------------------------*/
    t = (pow((Big)2, PBITS) - 1) / (2 * q);
    a = (pow((Big)2, PBITS - 1) - 1) / (2 * q);
    forever
    {
        n = rand(t);
        if (n < a) continue;
        p = 2 * n * q - 1;
        if (p % 24 != 11) continue;  // must be 2 mod 3, also 3 mod 8
        if (prime(p)) break;
    }

    cof = 2 * n;  //��Բ����������

    ecurve(0, 1, p, MR_PROJECTIVE);    // elliptic curve y^2=x^3+1 mod p����Ӱ����ϵͳ

   
    forever
    {
            cube = pow(randn2(),(p + 1) / 3); 
            cube = pow(cube,p - 1);
            if (!cube.isunity()) break;  
    }

    cout << "Cube root of unity= " << cube << endl;

    if (!(cube * cube * cube).isunity())
    {
        cout << "sanity check failed" << endl;
        exit(0);
    }

    sk_pkg.alpha = rand(q);
    sk_ts.beta = rand(q);
    
	cout << "sk_pkg.alpha= " << sk_pkg.alpha << endl;
	cout << "sk_ts.beta= " << sk_ts.beta << endl;
    
    /*---------------------------------------------------------���ɹ�������params----------------------------------------------*/

    //������Բ����������ԪP(g)
    forever
    {
        while (!params.P.set(randn()));
        params.P *= cof;
        if (!params.P.iszero()) break;
    }

    //������Բ����������ԪP1(g1)
	params.P1 = sk_pkg.alpha * params.P;


    //������Բ����������ԪP2(g2)
    forever
    {
        while (!params.P2.set(randn()));
        params.P2 *= cof;
        if (!params.P2.iszero()) {
			cout << "685" << params.P2 << endl;
            break;
        }
    }

    //������Բ����������ԪP3(g3)
    params.P3 = sk_ts.beta * params.P;

    //������Բ����������ԪP4(g4)
    forever
    {
        while (!params.P4.set(randn()));
        params.P4 *= cof;
        if (!params.P4.iszero()) break;
    }

    //������Բ������u[257]
    for (int i = 0; i < 257; i++) {
        forever
        {
            while (!params.u[i].set(randn()));
            params.u[i] *= cof;
            if (!params.u[i].iszero()) break;
        }
    }

    //������Բ������v[257]
    for (int i = 0; i < 257; i++) {
        forever
        {
            while (!params.v[i].set(randn()));
            params.v[i] *= cof;
            if (!params.v[i].iszero()) break;
        }
    }

    //������Բ������˫����ӳ�� e(g1,g2) �� e(g3,g4)
    ecap(params.P1, params.P2, q, cube, params.e_g1_g2);
    if (ecap(params.P1, params.P2, q, cube, params.e_g1_g2)) {
        cout << "ecap:e_g1_g2 success" << endl;
    }
    else {
        cout << "ecap:e_g1_g2 error" << endl;
    }

    ecap(params.P3, params.P4, q, cube, params.e_g3_g4);
    if (ecap(params.P3, params.P4, q, cube, params.e_g3_g4)) {
        cout << "ecap:e_g3_g4 success" << endl;
	}
	else {
		cout << "ecap:e_g3_g4 error" << endl;
	}


	/*---------------------------------------------------------��������params���� ���----------------------------------------------*/


	/*---------------------------------------------------------���ɣ�Alice �� Bob ǩ����Կ��ز���----------------------------------------------*/
	//����Aliceǩ����Կ����֮һ��g
    forever
    {
        while (!key_Alice.g.set(randn()));
        key_Alice.g *= cof;
        if (!key_Alice.g.iszero()) break;
    }

    //����Aliceǩ����Կ����֮һ��g_
    forever
    {
        while (!key_Alice.g_.set(randn()));
        key_Alice.g_ *= cof;
        if (!key_Alice.g_.iszero()) break;
    }



    // ����Aliceǩ����Կ����֮һ��h
    forever
    {
        while (!key_Alice.h.set(randn()));
        key_Alice.h *= cof;
        if (!key_Alice.h.iszero()) break;
    }

    // ����Aliceǩ����Կ����֮һ��h1
    forever
    {
        while (!key_Alice.h1.set(randn()));
        key_Alice.h1 *= cof;
        if (!key_Alice.h1.iszero()) break;
    }

    // ����Aliceǩ����Կ����֮һ��g1
    forever
    {
        while (!key_Alice.g1.set(randn()));
        key_Alice.g1 *= cof;
        if (!key_Alice.g1.iszero()) break;
    }
   
    //����Bobǩ����Կ����֮һ��g
    forever
    {
        while (!key_Bob.g.set(randn()));
        key_Bob.g *= cof;
        if (!key_Bob.g.iszero()) break;
    }

    //����Bobǩ����Կ����֮һ��g_
    forever
    {
        while (!key_Bob.g_.set(randn()));
        key_Bob.g_ *= cof;
        if (!key_Bob.g_.iszero()) break;
    }

    // ����Bobǩ����Կ����֮һ��h
    forever
    {
        while (!key_Bob.h.set(randn()));
        key_Bob.h *= cof;
        if (!key_Bob.h.iszero()) break;
    }

    // ����Bobǩ����Կ����֮һ��h1
    forever
    {
        while (!key_Bob.h1.set(randn()));
        key_Bob.h1 *= cof;
        if (!key_Bob.h1.iszero()) break;
    }

    // ����Bobǩ����Կ����֮һ��g1
    forever
    {
        while (!key_Bob.g1.set(randn()));
        key_Bob.g1 *= cof;
        if (!key_Bob.g1.iszero()) break;
    }
     /* ���Ĵ�С�Ա� */
    CT1111.c1 = P;
    CT1111.c2 = gid1;
	CT1111.V = P;



    /*-------------------------------------------���ɣ�PKG��˽Կ��TS��˽Կ��Alice ˽Կ��Bob˽Կ��ʱ������ST----------------------------------------------*/
    m1 = 363;
    m2 = 222;


    cout << "m1 = " << m1 << endl;
    cout << "m2 = " << m2 << endl;
    cout << "m1 + m2 = " << m1 + m2 << endl;

    key_Alice.Qid = map_to_point(Alice);
    key_Bob.Qid = map_to_point(Bob);

    QtTimeToBeDec = map_to_point(TimeToBeDec);
    QtTimeNow = map_to_point(TimeNow);
    
    
    sk_pkg.sk_pkg = sk_pkg.alpha * params.P2;
	sk_ts.sk_ts = sk_ts.beta * params.P4;


    SkGen(params, sk_pkg, q, key_Alice, cube);           //����Alice����Կ
    key_Alice.Z = key_Alice.z * key_Alice.g_;

	SkGen(params, sk_pkg, q, key_Bob, cube);           //����Bob����Կ
    key_Bob.Z = key_Bob.z * key_Bob.g_;

	StGen(params, sk_ts, q, QtTimeToBeDec, st_TimeNow);    //����ʱ�����ʱ���Ӧ������ST

    g0 = randn2();

    sign(cube, q, fid, key_Alice, m1_sign, m1, p);   //ǩ��m1
	sign(cube, q, fid, key_Alice, m2_sign, m2, p);   //ǩ��m2

    Enc(g0, m1, params, QtTimeToBeDec, q, CT1, m1_sign, key_Bob);     // ���� m1  
	Enc(g0, m2, params, QtTimeToBeDec, q, CT2, m2_sign, key_Bob);     // ���� m2

	EvalSignEnc(CT1, CT2, CT3);           // ǩ��״̬�£�m3 = m1+m2

    start_time = clock();
	Dec(CT3, key_Bob, st_TimeNow, m3, cube, g0, q, m3_sign);    // ����m3
    end_time = clock();
    t_dec = end_time - start_time;
    cout << "�Ƕ��߳̽���ʱ��Ϊ��" << t_dec<< "����" << endl;

    //start_time = clock();
    //DecMulthread(CT3, key_Bob, st_TimeNow, m3, cube, g0, q, m3_sign);    // ����m3
    //end_time = clock();
    //t_dec = end_time - start_time;
    //cout << "���߳̽���ʱ��Ϊ��" << t_dec << "����" << endl;

    /*
    start_time = clock();
    ecap(params.P1, params.P2, q, cube, params.e_g1_g2);
    ecap(params.P1, params.P2, q, cube, params.e_g1_g2);
    ecap(params.P1, params.P2, q, cube, params.e_g1_g2);
    ecap(params.P1, params.P2, q, cube, params.e_g1_g2);
    end_time = clock();
    t_dec = end_time - start_time;
    cout << "�Ƕ��߳����4��ʱ��Ϊ��" << t_dec << "����" << endl;

    start_time = clock();
    thread t1(ecap, params.P1, params.P2, q, cube, params.e_g1_g2);
    thread t2(ecap, params.P1, params.P2, q, cube, params.e_g1_g2);
    thread t3(ecap, params.P1, params.P2, q, cube, params.e_g1_g2);
    thread t4(ecap, params.P1, params.P2, q, cube, params.e_g1_g2);

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    end_time = clock();
    t_dec = end_time - start_time;
    cout << "���߳����4��ʱ��Ϊ��" << t_dec << "����" << endl;
    */


	cout << "m1 ���ģ�" << m1 << endl;  //���m1ԭʼ����
    cout << "m2 ���ģ�" << m1 << endl;  //���m2ԭʼ����
    cout << "m1+m2 ���ģ�" << m1 + m2 << endl;  //���ԭʼ���� m3 = m1 + m2

    cout << "m3��" << m3 <<endl;  //������ܺ� m3 = m1 + m2

	// m3_sign.X = m3_sign.X + m3_sign.X;  //�޸�ǩ���е�X
	// m3_sign.es = randn2();  //�޸�ǩ���е�es
    // m3 += 1;

	//��֤ m3
	if (verify(fid, key_Alice, m3_sign, cube, q, m3)) {
		
        cout << "��֤�ɹ�,m3Ϊ��ȷ�����" << endl;
	}
	else {
		cout << "��֤ʧ�ܣ�m3��ԭʼ���ݻ����ķ�ԭʼ���ݣ�����" << endl;

	}

    // ���ṹ��д���ļ�  
    outputFile1.write(reinterpret_cast<char*>(&CT3), sizeof(CT3));

    outputFile1.close(); // �ر��ļ�  
    std::cout << "�ṹ���ѳɹ�д���ļ�1" << std::endl;

    // ���ṹ��д���ļ�  
    outputFile2.write(reinterpret_cast<char*>(&st_TimeNow), sizeof(st_TimeNow));

    outputFile2.close(); // �ر��ļ�  
    std::cout << "�ṹ���ѳɹ�д���ļ�1" << std::endl;

    // ���ṹ��д���ļ�  
    outputFile3.write(reinterpret_cast<char*>(&CT1111), sizeof(CT1111));

    outputFile3.close(); // �ر��ļ�  
    std::cout << "�ṹ���ѳɹ�д���ļ�1" << std::endl;



    //return 0;
}


	