/*	SCCS Id: @(#)wizard.c	3.4	2003/02/18	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* wizard code - inspired by rogue code from Merlyn Leroy (digi-g!brian) */
/*	       - heavily modified to give the wiz balls.  (genat!mike)   */
/*	       - dewimped and given some maledictions. -3. */
/*	       - generalized for 3.1 (mike@bullns.on01.bull.ca) */

#include "hack.h"
#include "qtext.h"
#include "epri.h"

extern const int monstr[];

#ifdef OVLB

STATIC_DCL short FDECL(which_arti, (int));
STATIC_DCL boolean FDECL(mon_has_arti, (struct monst *,SHORT_P));
STATIC_DCL struct monst *FDECL(other_mon_has_arti, (struct monst *,SHORT_P));
STATIC_DCL struct obj *FDECL(on_ground, (SHORT_P));
STATIC_DCL boolean FDECL(you_have, (int));
STATIC_DCL long FDECL(target_on, (int,struct monst *));
STATIC_DCL long FDECL(strategy, (struct monst *));
STATIC_DCL void FDECL(wizgush, (int, int, genericptr_t));
STATIC_DCL void NDECL(dowizgush);

static int xprime = 0, yprime = 0;

static NEARDATA const int nasties[] = {
	PM_COCKATRICE, PM_ETTIN, PM_STALKER, PM_MINOTAUR, PM_RED_DRAGON,
	PM_BLACK_DRAGON, PM_GREEN_DRAGON, PM_OWLBEAR, PM_PURPLE_WORM,
	PM_ROCK_TROLL, PM_XAN, PM_GREMLIN, PM_UMBER_HULK, PM_VAMPIRE_LORD,
	PM_XORN, PM_ZRUTY, PM_ELF_LORD, PM_ELVENKING, PM_YELLOW_DRAGON,
	PM_LEOCROTTA, PM_BALUCHITHERIUM, PM_CARNIVOROUS_APE, PM_FIRE_GIANT,
	PM_COUATL, PM_CAPTAIN, PM_WINGED_GARGOYLE, PM_MASTER_MIND_FLAYER,
	PM_FIRE_ELEMENTAL, PM_JABBERWOCK, PM_ARCH_LICH, PM_OGRE_KING,
	PM_OLOG_HAI, PM_IRON_GOLEM, PM_OCHRE_JELLY, PM_GREEN_SLIME,
	PM_DISENCHANTER
	};

static NEARDATA const unsigned wizapp[] = {
	PM_HUMAN, PM_WATER_DEMON, PM_VAMPIRE,
	PM_RED_DRAGON, PM_TROLL, PM_UMBER_HULK,
	PM_XORN, PM_XAN, PM_COCKATRICE,
	PM_FLOATING_EYE,
	PM_GUARDIAN_NAGA,
	PM_TRAPPER
};

#endif /* OVLB */
#ifdef OVL0

/* If you've found the Amulet, make the Wizard appear after some time */
/* Also, give hints about portal locations, if amulet is worn/wielded -dlc */
void
amulet()
{
	struct monst *mtmp;
	struct trap *ttmp;
	struct obj *amu;

#if 0		/* caller takes care of this check */
	if (!u.uhave.amulet)
		return;
#endif
	if ((((amu = uamul) != 0 && amu->otyp == AMULET_OF_YENDOR) ||
	     ((amu = uwep) != 0 && amu->otyp == AMULET_OF_YENDOR))
	    && !rn2(15)) {
	    for(ttmp = ftrap; ttmp; ttmp = ttmp->ntrap) {
		if(ttmp->ttyp == MAGIC_PORTAL) {
		    int du = distu(ttmp->tx, ttmp->ty);
		    if (du <= 9)
			pline("%s hot!", Tobjnam(amu, "feel"));
		    else if (du <= 64)
			pline("%s very warm.", Tobjnam(amu, "feel"));
		    else if (du <= 144)
			pline("%s warm.", Tobjnam(amu, "feel"));
		    /* else, the amulet feels normal */
		    break;
		}
	    }
	}

	if (!flags.no_of_wizards)
		return;
	/* find Wizard, and wake him if necessary */
	for(mtmp = fmon; mtmp; mtmp = mtmp->nmon)
	    if (!DEADMONSTER(mtmp) && mtmp->iswiz && mtmp->msleeping && !rn2(40)) {
		mtmp->msleeping = 0;
		if (distu(mtmp->mx,mtmp->my) > 2)
		    You(
    "get the creepy feeling that somebody noticed your taking the Amulet."
		    );
		return;
	    }
}

#endif /* OVL0 */
#ifdef OVLB

int
mon_has_amulet(mtmp)
register struct monst *mtmp;
{
	register struct obj *otmp;

	for(otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
		if(otmp->otyp == AMULET_OF_YENDOR) return(1);
	return(0);
}

int
mon_has_special(mtmp)
register struct monst *mtmp;
{
	register struct obj *otmp;

	for(otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
		if(otmp->otyp == AMULET_OF_YENDOR ||
			is_quest_artifact(otmp) ||
			otmp->otyp == BELL_OF_OPENING ||
			otmp->otyp == CANDELABRUM_OF_INVOCATION ||
			otmp->otyp == SPE_BOOK_OF_THE_DEAD) return(1);
	return(0);
}

/*
 *	New for 3.1  Strategy / Tactics for the wiz, as well as other
 *	monsters that are "after" something (defined via mflag3).
 *
 *	The strategy section decides *what* the monster is going
 *	to attempt, the tactics section implements the decision.
 */
#define STRAT(w, x, y, typ) (w | ((long)(x)<<16) | ((long)(y)<<8) | (long)typ)

#define M_Wants(mask)	(mtmp->data->mflags3 & (mask))

STATIC_OVL short
which_arti(mask)
	register int mask;
{
	switch(mask) {
	    case M3_WANTSAMUL:	return(AMULET_OF_YENDOR);
	    case M3_WANTSBELL:	return(BELL_OF_OPENING);
	    case M3_WANTSCAND:	return(CANDELABRUM_OF_INVOCATION);
	    case M3_WANTSBOOK:	return(SPE_BOOK_OF_THE_DEAD);
	    default:		break;	/* 0 signifies quest artifact */
	}
	return(0);
}

/*
 *	If "otyp" is zero, it triggers a check for the quest_artifact,
 *	since bell, book, candle, and amulet are all objects, not really
 *	artifacts right now.	[MRS]
 */
STATIC_OVL boolean
mon_has_arti(mtmp, otyp)
	register struct monst *mtmp;
	register short	otyp;
{
	register struct obj *otmp;

	for(otmp = mtmp->minvent; otmp; otmp = otmp->nobj) {
	    if(otyp) {
		if(otmp->otyp == otyp)
			return(1);
	    }
	     else if(is_quest_artifact(otmp)) return(1);
	}
	return(0);

}

STATIC_OVL struct monst *
other_mon_has_arti(mtmp, otyp)
	register struct monst *mtmp;
	register short	otyp;
{
	register struct monst *mtmp2;

	for(mtmp2 = fmon; mtmp2; mtmp2 = mtmp2->nmon)
	    /* no need for !DEADMONSTER check here since they have no inventory */
	    if(mtmp2 != mtmp)
		if(mon_has_arti(mtmp2, otyp)) return(mtmp2);

	return((struct monst *)0);
}

STATIC_OVL struct obj *
on_ground(otyp)
	register short	otyp;
{
	register struct obj *otmp;

	for (otmp = fobj; otmp; otmp = otmp->nobj)
	    if (otyp) {
		if (otmp->otyp == otyp)
		    return(otmp);
	    } else if (is_quest_artifact(otmp))
		return(otmp);
	return((struct obj *)0);
}

STATIC_OVL boolean
you_have(mask)
	register int mask;
{
	switch(mask) {
	    case M3_WANTSAMUL:	return(boolean)(u.uhave.amulet);
	    case M3_WANTSBELL:	return(boolean)(u.uhave.bell);
	    case M3_WANTSCAND:	return(boolean)(u.uhave.menorah);
	    case M3_WANTSBOOK:	return(boolean)(u.uhave.book);
	    case M3_WANTSARTI:	return(boolean)(u.uhave.questart);
	    default:		break;
	}
	return(0);
}

STATIC_OVL long
target_on(mask, mtmp)
	register int mask;
	register struct monst *mtmp;
{
	register short	otyp;
	register struct obj *otmp;
	register struct monst *mtmp2;

	if(!M_Wants(mask))	return(STRAT_NONE);

	otyp = which_arti(mask);
	if(!mon_has_arti(mtmp, otyp)) {
	    if(you_have(mask))
		return(STRAT(STRAT_PLAYER, u.ux, u.uy, mask));
	    else if((otmp = on_ground(otyp)))
		return(STRAT(STRAT_GROUND, otmp->ox, otmp->oy, mask));
	    else if(!is_Rebel(mtmp->data) && (mtmp2 = other_mon_has_arti(mtmp, otyp)))
		return(STRAT(STRAT_MONSTR, mtmp2->mx, mtmp2->my, mask));
	}
	return(STRAT_NONE);
}

STATIC_OVL long
strategy(mtmp)
	register struct monst *mtmp;
{
	long strat, dstrat;

	if (!is_covetous(mtmp->data) ||
		/* perhaps a shopkeeper has been polymorphed into a master
		   lich; we don't want it teleporting to the stairs to heal
		   because that will leave its shop untended */
		(mtmp->isshk && inhishop(mtmp)))
	    return STRAT_NONE;

	switch((mtmp->mhp*3)/mtmp->mhpmax) {	/* 0-3 */

	   default:
	    case 0:	/* panic time - mtmp is almost snuffed */
			return(STRAT_HEAL);

	    case 1:	/* the wiz is less cautious */
			if(mtmp->data != &mons[PM_WIZARD_OF_YENDOR])
			    return(STRAT_HEAL);
			/* else fall through */

	    case 2:	dstrat = STRAT_HEAL;
			break;

	    case 3:	dstrat = STRAT_NONE;
			break;
	}

	if(flags.made_amulet)
	    if((strat = target_on(M3_WANTSAMUL, mtmp)) != STRAT_NONE)
		return(strat);

	if(u.uevent.invoked) {		/* priorities change once gate opened */

	    if((strat = target_on(M3_WANTSARTI, mtmp)) != STRAT_NONE)
		return(strat);
	    if((strat = target_on(M3_WANTSBOOK, mtmp)) != STRAT_NONE)
		return(strat);
	    if((strat = target_on(M3_WANTSBELL, mtmp)) != STRAT_NONE)
		return(strat);
	    if((strat = target_on(M3_WANTSCAND, mtmp)) != STRAT_NONE)
		return(strat);
	} else {

	    if((strat = target_on(M3_WANTSBOOK, mtmp)) != STRAT_NONE)
		return(strat);
	    if((strat = target_on(M3_WANTSBELL, mtmp)) != STRAT_NONE)
		return(strat);
	    if((strat = target_on(M3_WANTSCAND, mtmp)) != STRAT_NONE)
		return(strat);
	    if((strat = target_on(M3_WANTSARTI, mtmp)) != STRAT_NONE)
		return(strat);
	}
	return(dstrat);
}

int
tactics(mtmp)
	register struct monst *mtmp;
{
	long strat = strategy(mtmp);

	mtmp->mstrategy = (mtmp->mstrategy & STRAT_WAITMASK) | strat;

	switch (strat) {
	    case STRAT_HEAL:	/* hide and recover */
		/* if wounded, hole up on or near the stairs (to block them) */
		/* unless, of course, there are no stairs (e.g. endlevel) */
		mtmp->mavenge = 1; /* covetous monsters attack while fleeing */
		if (In_W_tower(mtmp->mx, mtmp->my, &u.uz) ||
			(mtmp->iswiz && !xupstair && !mon_has_amulet(mtmp))) {
		    if (!rn2(3 + mtmp->mhp/10)) (void) rloc(mtmp, FALSE);
		} else if (xupstair &&
			 (mtmp->mx != xupstair || mtmp->my != yupstair)) {
		    (void) mnearto(mtmp, xupstair, yupstair, TRUE);
		}
		/* if you're not around, cast healing spells */
		if (distu(mtmp->mx,mtmp->my) > (BOLT_LIM * BOLT_LIM))
		    if(mtmp->mhp <= mtmp->mhpmax - 8) {
			mtmp->mhp += rnd(8);
			return(1);
		    }
		/* fall through :-) */

	    case STRAT_NONE:	/* harrass */
		if (!rn2((!mtmp->mflee || mtmp->data == &mons[PM_BANDERSNATCH]) ? 5 : 33)) mnexto(mtmp);
		return(0);

	    default:		/* kill, maim, pillage! */
	    {
		long  where = (strat & STRAT_STRATMASK);
		xchar tx = STRAT_GOALX(strat),
		      ty = STRAT_GOALY(strat);
		int   targ = strat & STRAT_GOAL;
		struct obj *otmp;

		if(!targ) { /* simply wants you to close */
		    return(0);
		}
		if((u.ux == tx && u.uy == ty) || where == STRAT_PLAYER) {
		    /* player is standing on it (or has it) */
		    mnexto(mtmp);
		    return(0);
		}
		if(where == STRAT_GROUND) {
		    if(!MON_AT(tx, ty) || (mtmp->mx == tx && mtmp->my == ty)) {
			/* teleport to it and pick it up */
			rloc_to(mtmp, tx, ty);	/* clean old pos */

			if ((otmp = on_ground(which_arti(targ))) != 0) {
			    if (cansee(mtmp->mx, mtmp->my))
				pline("%s picks up %s.",
				    Monnam(mtmp),
				    (distu(mtmp->mx, mtmp->my) <= 5) ?
				     doname(otmp) : distant_name(otmp, doname));
			    obj_extract_self(otmp);
			    (void) mpickobj(mtmp, otmp);
			    return(1);
			} else return(0);
		    } else {
			/* a monster is standing on it - cause some trouble */
			if (!rn2(5)) mnexto(mtmp);
			return(0);
		    }
	        } else { /* a monster has it - 'port beside it. */
		    (void) mnearto(mtmp, tx, ty, FALSE);
		    return(0);
		}
	    }
	}
	/*NOTREACHED*/
	return(0);
}

void
aggravate()
{
	register struct monst *mtmp;

	for(mtmp = fmon; mtmp; mtmp = mtmp->nmon)
	    if (!DEADMONSTER(mtmp)) {
		mtmp->msleeping = 0;
		if(!mtmp->mcanmove && !rn2(5)) {
			mtmp->mfrozen = 0;
      if(mtmp->data != &mons[PM_GIANT_TURTLE] || !(mtmp->mflee))
			mtmp->mcanmove = 1;
		}
	    }
}

void
clonewiz()
{
	register struct monst *mtmp2;

	if ((mtmp2 = makemon(&mons[PM_WIZARD_OF_YENDOR],
				u.ux, u.uy, NO_MM_FLAGS)) != 0) {
	    mtmp2->msleeping = mtmp2->mtame = mtmp2->mpeaceful = 0;
	    if (!u.uhave.amulet && rn2(2)) {  /* give clone a fake */
		(void) add_to_minv(mtmp2, mksobj(FAKE_AMULET_OF_YENDOR,
					TRUE, FALSE));
	    }
	    mtmp2->m_ap_type = M_AP_MONSTER;
	    mtmp2->mappearance = wizapp[rn2(SIZE(wizapp))];
	    newsym(mtmp2->mx,mtmp2->my);
	}
}

/* also used by newcham() */
int
pick_nasty()
{
    /* To do?  Possibly should filter for appropriate forms when
       in the elemental planes or surrounded by water or lava. */
    return nasties[rn2(SIZE(nasties))];
}

/* create some nasty monsters, aligned or neutral with the caster */
/* a null caster defaults to a chaotic caster (e.g. the wizard) */
int
nasty(mcast)
	struct monst *mcast;
{
    register struct monst	*mtmp;
    register int	i, j, tmp;
    int castalign = (mcast ? mcast->data->maligntyp : -1);
    coord bypos;
    int count=0;

    if(!rn2(10) && Inhell) {
	msummon((struct monst *) 0);	/* summons like WoY */
	count++;
    } else {
	tmp = (u.ulevel > 3) ? u.ulevel/3 : 1; /* just in case -- rph */
	/* if we don't have a casting monster, the nasties appear around you */
	bypos.x = u.ux;
	bypos.y = u.uy;
	for(i = rnd(tmp); i > 0; --i)
	    for(j=0; j<20; j++) {
		int makeindex;

		/* Don't create more spellcasters of the monsters' level or
		 * higher--avoids chain summoners filling up the level.
		 */
		do {
		    makeindex = pick_nasty();
		} while(mcast && attacktype(&mons[makeindex], AT_MAGC) &&
			monstr[makeindex] >= monstr[mcast->mnum]);
		/* do this after picking the monster to place */
		if (mcast &&
		    !enexto(&bypos, mcast->mux, mcast->muy, &mons[makeindex]))
		    continue;
		if ((mtmp = makemon(&mons[makeindex],
				    bypos.x, bypos.y, NO_MM_FLAGS)) != 0) {
		    mtmp->msleeping = mtmp->mpeaceful = mtmp->mtame = 0;
		    set_malign(mtmp);
		} else /* GENOD? */
		    mtmp = makemon((struct permonst *)0,
					bypos.x, bypos.y, NO_MM_FLAGS);
		if(mtmp && (mtmp->data->maligntyp == 0 ||
		            sgn(mtmp->data->maligntyp) == sgn(castalign)) ) {
		    count++;
		    break;
		}
	    }
    }
    return count;
}

/*	Let's resurrect the wizard, for some unexpected fun.	*/
void
resurrect()
{
	struct monst *mtmp, **mmtmp;
	long elapsed;
	const char *verb;

	if (!flags.no_of_wizards) {
	    /* make a new Wizard */
	    verb = "kill";
	    mtmp = makemon(&mons[PM_WIZARD_OF_YENDOR], u.ux, u.uy, MM_NOWAIT);
	} else {
	    /* look for a migrating Wizard */
	    verb = "elude";
	    mmtmp = &migrating_mons;
	    while ((mtmp = *mmtmp) != 0) {
		if (mtmp->iswiz &&
			/* if he has the Amulet, he won't bring it to you */
			!mon_has_amulet(mtmp) &&
			(elapsed = monstermoves - mtmp->mlstmv) > 0L) {
		    mon_catchup_elapsed_time(mtmp, elapsed);
		    if (elapsed >= LARGEST_INT) elapsed = LARGEST_INT - 1;
		    elapsed /= 50L;
		    if (mtmp->msleeping && rn2((int)elapsed + 1))
			mtmp->msleeping = 0;
		    if (mtmp->mfrozen == 1) /* would unfreeze on next move */
			mtmp->mfrozen = 0,  mtmp->mcanmove = 1;
		    if (mtmp->mcanmove && !mtmp->msleeping) {
			*mmtmp = mtmp->nmon;
			mon_arrive(mtmp, TRUE);
			/* note: there might be a second Wizard; if so,
			   he'll have to wait til the next resurrection */
			break;
		    }
		}
		mmtmp = &mtmp->nmon;
	    }
	}

	if (mtmp) {
		mtmp->msleeping = mtmp->mtame = mtmp->mpeaceful = 0;
		set_malign(mtmp);
		pline("A voice booms out...");
		verbalize("So thou thought thou couldst %s me, fool.", verb);
	}

}

STATIC_PTR void
dowizdarken()
{
	register struct monst *mtmp;
	register struct trap *ttmp;
	int cx, cy;
	
	for(cx = 0; cx < COLNO; cx++){
		for(cy = 0; cy < ROWNO; cy++){
			levl[cx][cy].lit = 0;
			snuff_light_source(cx, cy);
			if(!rn2(3)) switch(rn2(5)) {
				case 2:(void) makemon(&mons[PM_SHADE], cx, cy, NO_MM_FLAGS);break;
				case 3:(void) makemon(&mons[PM_BLACK_LIGHT], cx, cy, NO_MM_FLAGS);break;
				case 4:(void) makemon(&mons[PM_WRAITH], cx, cy, NO_MM_FLAGS);break;
				case 5:(void) makemon(rn2(10) ? &mons[PM_SHADE] : &mons[PM_DARKNESS_GIVEN_HUNGER], cx, cy, NO_MM_FLAGS);break;
				default:(void) makemon(mkclass(S_WRAITH, G_NOHELL|G_HELL), cx, cy, NO_MM_FLAGS);break;
			}
		}
	}
	if (!Blind) {
	    vision_recalc(2);

	    /* replace ball&chain */
	    if (Punished)
		move_bc(0, 0, uball->ox, uball->oy, uchain->ox, uchain->oy);
	}
}

void
dowizgush() /* Gushing forth along LOS from (u.ux, u.uy) and random other spots */
{
	int madepool = 0,cx,cy;
	
	xprime = u.ux;
	yprime = u.uy;
	do_clear_area(u.ux, u.uy, 9, wizgush, (genericptr_t)&madepool);
	pline("Water sprays all over you.");
	water_damage(invent, FALSE, FALSE, FALSE);
	for(cx = 0; cx < COLNO; cx++){
		for(cy = 0; cy < ROWNO; cy++){
			if(!rn2(100)){
				madepool = 0;
				xprime = cx;
				yprime = cy;
				do_clear_area(cx, cy, 9, wizgush, (genericptr_t)&madepool);
			}
		}
	}
}

STATIC_PTR void
wizgush(cx, cy, poolcnt)
int cx, cy;
genericptr_t poolcnt;
{
	register struct monst *mtmp;
	register struct trap *ttmp;

	if (((cx+cy)%2) || 
	    (rn2(1 + distmin(xprime, yprime, cx, cy)))  ||
	    (levl[cx][cy].typ != ROOM) ||
	    (boulder_at(cx, cy)) || nexttodoor(cx, cy))
		return;

	if ((ttmp = t_at(cx, cy)) != 0 && !delfloortrap(ttmp))
		return;

	/* Put a pool at cx, cy */
	levl[cx][cy].typ = POOL;
	/* No kelp! */
	del_engr_ward_at(cx, cy);
	water_damage(level.objects[cx][cy], FALSE, TRUE, FALSE);

	if ((mtmp = m_at(cx, cy)) != 0){
		(void) minliquid(mtmp);
	}else{
		if(!rn2(3)) switch(rn2(In_hell(&u.uz) ? 7 : 5)) {
			case 2:(void) makemon(&mons[PM_KRAKEN], cx, cy, NO_MM_FLAGS);
			case 3:(void) makemon(&mons[PM_WATER_ELEMENTAL], cx, cy, NO_MM_FLAGS);
			case 4:(void) makemon(&mons[PM_DEEPER_ONE], cx, cy, NO_MM_FLAGS);
			case 5:(void) makemon(&mons[PM_WATER_DEMON], cx, cy, NO_MM_FLAGS);
			case 6:(void) makemon(&mons[PM_UMBER_HULK], cx, cy, NO_MM_FLAGS);
			default:(void) makemon(mkclass(S_EEL, G_NOHELL|G_HELL), cx, cy, NO_MM_FLAGS);
		}
		newsym(cx,cy);
	}
}

/*	Here, we make trouble for the poor shmuck who actually	*/
/*	managed to do in the Wizard.				*/
void
intervene()
{
	int which = Is_astralevel(&u.uz) ? rnd(4) : rn2(6);
	/* cases 0 and 5 don't apply on the Astral level */
	switch (which) {
	    case 0:
	    case 1:	You_feel("vaguely nervous.");
			break;
	    case 2:	if (!Blind)
			    You("notice a %s glow surrounding you.",
				  hcolor(NH_BLACK));
			rndcurse();
			break;
	    case 3:	aggravate();
			break;
	    case 4:	(void)nasty((struct monst *)0);
			break;
	    case 5:	resurrect();
			break;
	}
}

void
wizdead()
{
	flags.no_of_wizards--;
	if (!u.uevent.udemigod) {
		u.uevent.udemigod = TRUE;
		u.udg_cnt = rn1(250, 50);
	}
}

const char * const random_insult[] = {
	"antic",
	"blackguard",
	"caitiff",
	"chucklehead",
	"coistrel",
	"craven",
	"cretin",
	"cur",
	"dastard",
	"demon fodder",
	"dimwit",
	"dolt",
	"fool",
	"footpad",
	"imbecile",
	"knave",
	"maledict",
	"miscreant",
	"niddering",
	"poltroon",
	"rattlepate",
	"reprobate",
	"scapegrace",
	"varlet",
	"villein",	/* (sic.) */
	"wittol",
	"worm",
	"wretch",
};

const char * const random_malediction[] = {
	"Hell shall soon claim thy remains,",
	"I chortle at thee, thou pathetic",
	"Prepare to die, thou",
	"Resistance is useless,",
	"Surrender or die, thou",
	"There shall be no mercy, thou",
	"Thou shalt repent of thy cunning,",
	"Thou art as a flea to me,",
	"Thou art doomed,",
	"Thy fate is sealed,",
	"Verily, thou shalt be one dead"
};

const char * const random_garlandism[] = {
	"Cornelia will be mine!",
	"Thou art but a worthless lapdog of the King!",
	"Do you have any idea who you're messing with!?",
	"You really think you have what it takes to cross swords with ME?",
	"I, Garland, will knock you all down!!"
};
const char * const random_chaos_garlandism[] = {
	"Do you remember me!?",
	"Two thousand years from now...you killed me.\n  Now, it's my turn!",
	"I!  AM!  GAR! LAAAAND!",
	"Oh, you did defeat me then.  But now I shall kill you, and live forever!",
	"In two thousand years, I will remember none of this.",
	"I, Garland, will knock you all down!!"
};
const char * const random_chaosism[] = {
	"Do you remember me!?",																	//0
	"Two thousand years from now...you killed me.\n  Now, YOU will die!",					//1
	"Oh, you did defeat me then.  But now I shall kill you, and live FOREVER.",				//2
	"In two thousand years, I will remember none of this.",									//3
	"I, Chaos, will knock you all down!!",													//4
																							///
	"Many times have you kill me hence!",													//5
	"But I have ever been reborn here!",													//6
	"Many times have you followed me!",														//7
	"Many times, have you DIED!",															//8
	"So even as you die again and again, I shall return!\n\
	Born forever into this endless circle that I have created, here, writ with your BLOOD!"	//9

};
const char * const random_apollyon[] = {
	"Tremble, mortal!", /*0*/
	"Tremble, mortal!", /*1*/
	"Tremble, mortal!", /*2*/
	"Kneel.", /*3*/
	"Bow before me!", /*4*/
	"You viper!", /*5*/
	"Cower, mortal!", /*6*/
	"Cower, mortal!", /*7*/
	"Tremble, mortal, you face the Angel of the Pit!", /*8*/
	"Bow before me, mortal.  For I am Apollyon.", /*9*/
	"You viper.  I shall sentence you to Gehennom!", /*10*/
	"Cower, mortal.  I am the Jailer of Heaven!" /*11*/
};

/* Insult or intimidate the player */
void
cuss(mtmp)
register struct monst	*mtmp;
{
	if (mtmp->iswiz) {
	    if (!rn2(5))  /* typical bad guy action */
		pline("%s laughs fiendishly.", Monnam(mtmp));
	    else
		if (u.uhave.amulet && !rn2(SIZE(random_insult)))
		    verbalize("Relinquish the amulet, %s!",
			  random_insult[rn2(SIZE(random_insult))]);
		else if (u.uhp < 5 && !rn2(2))	/* Panic */
		    verbalize(rn2(2) ?
			  "Even now thy life force ebbs, %s!" :
			  "Savor thy breath, %s, it be thy last!",
			  random_insult[rn2(SIZE(random_insult))]);
		else if (mtmp->mhp < 5 && !rn2(2))	/* Parthian shot */
		    verbalize(rn2(2) ?
			      "I shall return." :
			      "I'll be back.");
		else
		    verbalize("%s %s!",
			  random_malediction[rn2(SIZE(random_malediction))],
			  random_insult[rn2(SIZE(random_insult))]);
	}
	else if(mtmp->data == &mons[PM_CHAOS]){
		if(mtmp->mextra[1]<5){
			verbalize("%s", random_chaosism[mtmp->mextra[1]+5]);
			mtmp->mextra[1]++;
		}
		else verbalize("%s", random_chaosism[rn2(5)]);
	}
	else if(mtmp->data == &mons[PM_GARLAND]){
		verbalize("%s", random_chaos_garlandism[rn2(SIZE(random_chaos_garlandism))]);
	}
	else if(mtmp->data == &mons[PM_SIR_GARLAND]){
		verbalize("%s", random_garlandism[rn2(SIZE(random_garlandism))]);
	}
	else if(mtmp->data == &mons[PM_APOLLYON]){
		verbalize("%s", random_apollyon[rn2(SIZE(random_apollyon))]);
	}
	else if(is_lminion(mtmp)) {
		com_pager(rn2(QTN_ANGELIC - 1 + (Hallucination ? 1 : 0)) +
			      QT_ANGELIC);
	} else {
	    if (!rn2(5))
		pline("%s casts aspersions on your ancestry.", Monnam(mtmp));
	    else
	        com_pager(rn2(QTN_DEMONIC) + QT_DEMONIC);
	}
}

#endif /* OVLB */

/*wizard.c*/
