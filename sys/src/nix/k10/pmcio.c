/*
 *  Performance counters non port part
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"amd64.h"
#include	"../port/pmc.h"


/* non portable, for intel will be CPUID.0AH.EDX 
 */

enum {
	PeNreg		= 4,	/* Number of Pe/Pct regs */
};

int
pmcnregs(void)
{
	/* could run CPUID to see if there are registers,
	 * PmcMaxCtrs
 	*/
	return PeNreg;
}

//PeHo|PeGo
#define PeAll	(PeOS|PeUsr)	
#define SetEvMsk(v, e) ((v)|(((e)&PeEvMskL)|(((e)<<(PeEvMsksh-8))&PeEvMskH)))
#define SetUMsk(v, u) ((v)|(((u)<<8ull)&PeUnMsk))

#define GetEvMsk(e) (((e)&PeEvMskL)|(((e)&PeEvMskH)>>(PeEvMsksh-8)))
#define GetUMsk(u) (((u)&PeUnMsk)>>8ull)

static int
pmcuserenab(int enable)
{
	u64int cr4;

	cr4 = cr4get();
	if (enable){
		cr4 |= Pce;
	} else
		cr4 &=  ~Pce;
	cr4put(cr4);
	return cr4&Pce;
}

PmcCtlCtrId pmcids[] = {
	{"locked instr", "0x024 0x1"},
	{"SMI intr", "0x02b 0"},
	{"data access", "0x040 0x0"},
	{"data miss", "0x041 0x0"},
	{"L1 DTLB miss", "0x045 0x7"},	//L2 hit
	{"L2 DTLB miss", "0x046 0x7"},
	{"L1 DTLB hit", "0x04d 0x3"},
	{"L2 hit", "0x07d 0x3f"},
	{"L2 miss", "0x07e 0xf"},
	{"instr miss", "0x081 0x0"},
	{"L1 ITLB miss", "0x084 0"},	//L2 hit
	{"L2 ITLB miss", "0x085 0x3"},
	{"DRAM access", "0x0e0 0x3f"},
	{"L3 miss", "0x4e1 0x3"},	//one core, can be set to more
	{"", ""},
};

int
pmctrans(PmcCtl *p)
{
	PmcCtlCtrId *pi;

	for (pi = &pmcids[0]; pi->portdesc[0] != '\0'; pi++){
		if ( strncmp(p->descstr, pi->portdesc, strlen(pi->portdesc)) == 0){
			strncpy(p->descstr, pi->archdesc, strlen(pi->archdesc) + 1);
			return 0;
		}
	}
	return 1;
}

static int
getctl(PmcCtl *p, u32int regno)
{
	u64int r, e, u;

	r = rdmsr(regno + PerfEvtbase);
	p->enab = (r&PeCtEna) != 0;
	p->user = (r&PeUsr) != 0;
	p->os = (r&PeOS) != 0;
	e = GetEvMsk(r);
	u = GetUMsk(r);
	//TODO inverse translation
	snprint(p->descstr, KNAMELEN, "%#ullx %#ullx", e, u);
	p->nodesc = 0;
	return 0;
}

int
pmcanyenab(void)
{
	int i;
	PmcCtl p;

	for (i = 0; i < pmcnregs(); i++) {
		if (getctl(&p, i) < 0)
			return -1;
		if (p.enab)
			return 1;
	}

	return 0;
}

extern int pmcdebug;

static int
setctl(PmcCtl *p, int regno)
{
	u64int v, e, u;
	char *toks[2];
	char str[KNAMELEN];

	if (regno >= pmcnregs())
		error("invalid reg");

	v = rdmsr(regno + PerfEvtbase);
	v &= PeEvMskH|PeEvMskL|PeCtEna|PeOS|PeUsr|PeUnMsk;
	if (p->enab != PmcCtlNullval)
		if (p->enab)
			v |= PeCtEna;
		else
			v &= ~PeCtEna;

	if (p->user != PmcCtlNullval)
		if (p->user)
			v |= PeUsr;
		else
			v &= ~PeUsr;

	if (p->os != PmcCtlNullval)
		if (p->os)
			v |= PeOS;
		else
			v &= ~PeOS;

	if (pmctrans(p) < 0)
		return -1;

	if (p->nodesc == 0) {
		memmove(str, p->descstr, KNAMELEN);
		if (tokenize(str, toks, 2) != 2)
			return -1;
		e = atoi(toks[0]);
		u = atoi(toks[1]);
		v &= ~(PeEvMskL|PeEvMskH|PeUnMsk);
		v |= SetEvMsk(v, e);
		v |= SetUMsk(v, u);
	}
	if (p->reset != PmcCtlNullval && p->reset) {
		v = 0;
		wrmsr(regno+ PerfCtrbase, 0);
		p->reset = PmcCtlNullval; /* only reset once */
	}
	wrmsr(regno+ PerfEvtbase, v);
	pmcuserenab(pmcanyenab());
	if (pmcdebug) {
		v = rdmsr(regno+ PerfEvtbase);
		print("conf pmc[%#ux]: %#llux\n", regno, v);
	}
	return 0;
}

int
pmcctlstr(char *str, int nstr, PmcCtl *p)
{
	int ns;

	ns = 0;
	if (p->enab && p->enab != PmcCtlNullval)
		ns += snprint(str + ns, nstr - ns, "enable\n");
	else
		ns += snprint(str + ns, nstr - ns, "disable\n");
		
	if (p->user && p->user != PmcCtlNullval)
		ns += snprint(str + ns, nstr - ns, "user\n");
	if (p->os && p->user != PmcCtlNullval)
		ns += snprint(str + ns, nstr - ns, "os\n");
	
	//TODO, inverse pmctrans?
	if(!p->nodesc)
		ns += snprint(str + ns, nstr - ns, "%s\n", p->descstr);
	else 
		ns += snprint(str + ns, nstr - ns, "no desc\n");
	return ns;
}

int
pmcdescstr(char *str, int nstr)
{
	PmcCtlCtrId *pi;
	int ns;

	ns = 0;

	for (pi = &pmcids[0]; pi->portdesc[0] != '\0'; pi++)
		ns += snprint(str + ns, nstr - ns, "%s\n",pi->portdesc);
	return ns;
}

static u64int
getctr(u32int regno)
{
	return rdmsr(regno + PerfCtrbase);
}

static int
setctr(u64int v, u32int regno)
{
	wrmsr(regno + PerfCtrbase, v);
	return 0;
}

static int
notstale(void *x)
{
	PmcCtr *p;
	p = (PmcCtr *)x;
	return !p->stale;
}

/*
 *	As it is now, it sends an ipi if the proccessor is an ocuppied AC or a TC
 *	to update the counter, in case the processor is idle. Probably not needed for TC
 *	as it will be updated every time we cross the kernel boundary, but we are doing
 *	it now just in case it is idle or not being updated
 *	NB: this function releases the ilock
 */


static PmcWait*
newpmcw(void)
{
	PmcWait *w;

	w = malloc(sizeof (PmcWait));
	w->ref = 1;
	return w;
}

static void
pmcwclose(PmcWait *w)
{
	if(decref(w))
		return;

	free(w);
}

static void
waitnotstale(Mach *mp, PmcCtr *p)
{
	PmcWait *w;

	w = newpmcw();
	w->next = p->wq;
	p->wq = w;
	incref(w);
	iunlock(&mp->pmclock);
	apicipi(mp->apicno);
	if(waserror()){
		pmcwclose(w);
		nexterror();
	}
	sleep(&w->r, notstale, p);
	poperror();
	pmcwclose(w);
}

u64int
pmcgetctr(u32int coreno, u32int regno)
{
	PmcCtr *p;
	Mach *mp;
	u64int v;

	if(coreno == m->machno){
		v = getctr(regno);
		if (pmcdebug) {
			print("int getctr[%#ux, %#ux] = %#llux\n", regno, coreno, v);
		}
		return v;
	}

	mp = sys->machptr[coreno];
	p = &mp->pmc[regno];
	ilock(&mp->pmclock);
	p->ctrset |= PmcGet;
	p->stale = 1;
	if(mp->proc != nil || mp->nixtype != NIXAC){
		waitnotstale(mp, p);
		ilock(&mp->pmclock);
	}
	v = p->ctr;
	iunlock(&mp->pmclock);
	if (pmcdebug) {
		print("ext getctr[%#ux, %#ux] = %#llux\n", regno, coreno, v);
	}
	return v;
}

int
pmcsetctr(u32int coreno, u64int v, u32int regno)
{
	PmcCtr *p;
	Mach *mp;

	if(coreno == m->machno){
		if (pmcdebug) {
			print("int getctr[%#ux, %#ux] = %#llux\n", regno, coreno, v);
		}
		return setctr(v, regno);
	}

	mp = sys->machptr[coreno];
	p = &mp->pmc[regno];
	if (pmcdebug) {
		print("ext setctr[%#ux, %#ux] = %#llux\n", regno, coreno, v);
	}
	ilock(&mp->pmclock);
	p->ctr = v;
	p->ctrset |= PmcSet;
	p->stale = 1;
	if(mp->proc != nil || mp->nixtype != NIXAC)
		waitnotstale(mp, p);
	else
		iunlock(&mp->pmclock);
	return 0;
}

static void
ctl2ctl(PmcCtl *dctl, PmcCtl *sctl)
{
	if(sctl->enab != PmcCtlNullval)
		dctl->enab = sctl->enab;
	if(sctl->user != PmcCtlNullval)
		dctl->user = sctl->user;
	if(sctl->os != PmcCtlNullval)
		dctl->os = sctl->os;
	if(sctl->nodesc == 0) {
		memmove(dctl->descstr, sctl->descstr, KNAMELEN);
		dctl->nodesc = 0;
	}
}

int
pmcsetctl(u32int coreno, PmcCtl *pctl, u32int regno)
{
	PmcCtr *p;
	Mach *mp;

	if(coreno == m->machno)
		return setctl(pctl, regno);

	mp = sys->machptr[coreno];
	p = &mp->pmc[regno];
	ilock(&mp->pmclock);
	ctl2ctl(&p->PmcCtl, pctl);
	p->ctlset |= PmcSet;
	p->stale = 1;
	if(mp->proc != nil || mp->nixtype != NIXAC)
		waitnotstale(mp, p);
	else
		iunlock(&mp->pmclock);
	return 0;
}

int
pmcgetctl(u32int coreno, PmcCtl *pctl, u32int regno)
{
	PmcCtr *p;
	Mach *mp;

	if(coreno == m->machno)
		return getctl(pctl, regno);

	mp = sys->machptr[coreno];
	p = &mp->pmc[regno];

	ilock(&mp->pmclock);
	p->ctlset |= PmcGet;
	p->stale = 1;
	if(mp->proc != nil || mp->nixtype != NIXAC){
		waitnotstale(mp, p);
		ilock(&mp->pmclock);
	}
	memmove(pctl, &p->PmcCtl, sizeof(PmcCtl));
	iunlock(&mp->pmclock);
	return 0;
}

void
pmcupdate(Mach *m)
{
	PmcCtr *p;
	int i, maxct, wk;
	PmcWait *w;

	maxct = pmcnregs();
	for (i = 0; i < maxct; i++) {
		p = &m->pmc[i];
		ilock(&m->pmclock);
		if(p->ctrset & PmcSet)
			setctr(p->ctr, i);
		if(p->ctlset & PmcSet)
			setctl(p, i);
		p->ctr = getctr(i);
		getctl(p, i);
		p->ctrset = PmcIgn;
		p->ctlset = PmcIgn;
		wk = p->stale;
		p->stale = 0;
		if(wk){
			for(w = p->wq; w != nil; w = w->next){
				p->wq = w->next;
				wakeup(&w->r);
				pmcwclose(w);
			}
		}
		iunlock(&m->pmclock);
	}
}

