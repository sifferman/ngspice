/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1985 Thomas L. Quarles
**********/

#include "ngspice/ngspice.h"
#include "ngspice/iferrmsg.h"
#include "ngspice/inpdefs.h"
#include "inpxx.h"

static struct osdi_binding {
    char *type_name;
    int level;
    char *module_name;
    char *polarity_parameter;
    char *polarity_value;
    char *version;
} *osdi_bindings;
static int osdi_binding_count;


void
INPosdiBind(const char *type_name, int level, const char *module_name,
            const char *polarity_parameter, const char *polarity_value, const char *version)
{
    osdi_bindings = TREALLOC(struct osdi_binding, osdi_bindings, osdi_binding_count + 1);
    osdi_bindings[osdi_binding_count++] = (struct osdi_binding) {
        copy(type_name), level, copy(module_name), copy(polarity_parameter), copy(polarity_value), copy(version)
    };
}


/* Prefer the last loaded binding matching the card version, then the last
   loaded binding */
static const struct osdi_binding *
find_osdi_binding(const char *type_name, int level, char *line, char **err)
{
    char card_version[100] = "";
    if (strstr(line, "version"))
        INPfindVer(line, card_version);

    const struct osdi_binding *loaded_match = NULL, *unloaded_match = NULL;
    const struct osdi_binding *fallback_loaded = NULL, *fallback = NULL;
    bool several_loaded = FALSE;
    for (int i = osdi_binding_count - 1; i >= 0; i--) {
        const struct osdi_binding *binding = &osdi_bindings[i];
        if (binding->level != level || !cieq(binding->type_name, type_name))
            continue;
        const bool is_loaded = INPtypelook(binding->module_name) >= 0;
        const bool matches_card = *card_version && binding->version && prefix(binding->version, card_version);
        if (matches_card && is_loaded && !loaded_match)
            loaded_match = binding;
        if (matches_card && !is_loaded && !unloaded_match)
            unloaded_match = binding;
        if (is_loaded && fallback_loaded && strcasecmp(binding->module_name, fallback_loaded->module_name) != 0)
            several_loaded = TRUE;
        if (is_loaded && !fallback_loaded)
            fallback_loaded = binding;
        if (!fallback)
            fallback = binding;
    }

    if (loaded_match)
        return loaded_match;
    if (!fallback_loaded)
        return unloaded_match ? unloaded_match : fallback;
    const char *fallback_name = fallback_loaded->module_name;
    if (err && unloaded_match)
        *err = INPerrCat(*err, tprintf("%s level %d: module %s for version %s not loaded, falling back to %s",
                                       type_name, level, unloaded_match->module_name, card_version, fallback_name));
    else if (err && *card_version && (fallback_loaded->version || several_loaded))
        *err = INPerrCat(*err, tprintf("%s level %d: version %s not found, falling back to %s",
                                       type_name, level, card_version, fallback_name));
    else if (err && !*card_version && several_loaded)
        *err = INPerrCat(*err, tprintf("%s level %d: version not given, defaulting to %s",
                                       type_name, level, fallback_name));
    return fallback_loaded;
}


static int
find_loaded_osdi_type(const char *type_name, int level, char *line)
{
    const struct osdi_binding *binding = find_osdi_binding(type_name, level, line, NULL);
    return binding ? INPtypelook(binding->module_name) : -1;
}


/* Return an error message, or NULL */
static char *
find_osdi_type(const char *type_name, int level, char *line, int *type, char *unsupported_level_message)
{
    const struct osdi_binding *binding = find_osdi_binding(type_name, level, line, NULL);
    if (!binding)
        return INPmkTemp(unsupported_level_message);
    *type = INPtypelook(binding->module_name);
    if (*type < 0)
        return tprintf("%s level %d needs Verilog-A module %s loaded with pre_osdi\n",
                       type_name, level, binding->module_name);
    return NULL;
}


bool
INPfindOsdiBinding(const char *module_name, const char *type_name, char *line,
                   char **polarity_parameter, char **polarity_value, char **err)
{
    bool binds_type_name = FALSE;
    for (int i = 0; i < osdi_binding_count && !binds_type_name; i++)
        binds_type_name = cieq(osdi_bindings[i].type_name, type_name);
    if (!binds_type_name)
        return FALSE;

    int level;
    txfree(INPfindLev(line, &level));
    const struct osdi_binding *binding = find_osdi_binding(type_name, level, line, err);
    if (!binding || strcasecmp(binding->module_name, module_name) != 0)
        return FALSE;
    *polarity_parameter = binding->polarity_parameter;
    *polarity_value = binding->polarity_value;
    return TRUE;
}

/*--------------------------------------------------------------
 * This fcn takes the model card & examines it.  Depending upon
 * model type, it parses the model line, and then calls
 * INPmakeMod to stick the model name into the model list.
 * Note that multi-line models are handled in the calling fcn
 * (INPpas1).
 *-------------------------------------------------------------*/
char *INPdomodel(CKTcircuit *ckt, struct card *image, INPtables * tab)
{

    char *modname;
    int type = -1;
    int lev, error1=0;
    char ver[100];
    char *type_name;
    char *err = NULL;
    char *line;
    char *val;
    double rval=0, lval=0;

    NG_IGNORE(ckt);

    line = image->line;

#ifdef TRACE
    printf("In INPdomodel, examining line %s . . . \n", line);
#endif

    INPgetTok(&line, &modname, 1);	/* throw away '.model' */
    tfree(modname);
    INPgetNetTok(&line, &modname, 1);      /* get model name */
    INPinsert(&modname, tab);	   /* stick model name into table */
    INPgetTok(&line, &type_name, 1);     /* get model type */

    /*  -----  Check if model is a BJT --------- */
    if (strcmp(type_name, "npn") == 0 || strcmp(type_name, "pnp") == 0) {
			err = INPfindLev(line,&lev);
			switch(lev) {
				case 0:
				case 1:
				case 2:
					type = INPtypelook("BJT");
					if (type < 0) {
			  		  err = INPmkTemp(
			  			  "Device type BJT not available in this binary\n");
					}
				break;
				case 4:
				case 9:
					 type = INPtypelook("VBIC");
				if(type < 0) {
				    err = INPmkTemp(
					    "Device type VBIC not available in this binary\n");
				}
				break;
				default: /* placeholder; use level 4 for the next model */
				err = find_osdi_type(type_name, lev, line, &type,
				  "Only BJT levels 1-2, 4, 9 and levels bound with osdi_bind are supported in this binary\n");
				break;

			}
			INPmakeMod(modname, type, image);
    } /* end if ((strcmp(typename, "npn") == 0) || (strcmp(typename, "pnp") == 0)) */

    /*  --------  Check if model is a diode --------- */
    else if (strcmp(type_name, "d") == 0) {
			txfree(INPfindLev(line, &lev));	/* model Diode checks its own level */
			type = find_loaded_osdi_type(type_name, lev, line);
			if (type < 0)
			    type = INPtypelook("Diode");
			if (type < 0) {
			    err =
				INPmkTemp
				("Device type Diode not available in this binary\n");
			}
			INPmakeMod(modname, type, image);
    } /*   else if (strcmp(typename, "d") == 0) {  */

    /*  --------  Check if model is a jfet --------- */
    else if (strcmp(type_name, "njf") == 0  ||
	     strcmp(type_name, "pjf") == 0) {
			err = INPfindLev(line, &lev);
			switch (lev) {
			case 0:
			case 1:
			    type = INPtypelook("JFET");
			    if (type < 0) {
				err =
				    INPmkTemp
				    ("Device type JFET not available in this binary\n");
			    }
			    break;
			case 2:
			    type = INPtypelook("JFET2");
			    if (type < 0) {
				err =
				    INPmkTemp
				    ("Device type JFET2 not available in this binary\n");
			    }
			    break;
			default:		/* placeholder; use level 3 for the next model */
			    err =
				INPmkTemp
				("Only JFET device levels 1-2 are supported in this binary\n");
			    break;
			}
			INPmakeMod(modname, type, image);
    }  /*   end  else if ((strcmp(typename, "njf") == 0) */

    /*  --------  Check if model is a MES or an HFET --------- */
    else if (strcmp(type_name, "nmf")   == 0  ||
	     strcmp(type_name, "pmf")   == 0  ||
	     strcmp(type_name, "nhfet") == 0  ||
	     strcmp(type_name, "phfet") == 0) {
			  err = INPfindLev( line, &lev );
			  switch ( lev )
			  {
				case 0:
				case 1:
					type = INPtypelook("MES");
					if (type < 0)
					{
						err = INPmkTemp("Device type MES not available\n");
					}
					break;
				case 2:
					type = INPtypelook("MESA");
					if (type < 0)
					{
						err = INPmkTemp("Device type MESA not available\n");
					}
					break;
				case 3:
					type = INPtypelook("MESA");
					if (type < 0)
					{
						err = INPmkTemp("Device type MESA not available\n");
					}
					break;
				case 4:
					type = INPtypelook("MESA");
					if ( type < 0)
					{
						err = INPmkTemp(" Device type MESA not available\n");
					}
					break;
				case 5:
					type = INPtypelook("HFET1");
					if ( type < 0)
					{
						err = INPmkTemp(" Device type HFET1 not available\n");
					}
					break;
				case 6:
					type = INPtypelook("HFET2");
					if ( type < 0)
					{
						err = INPmkTemp(" Device type HFET2 not available in this binary\n");
					}
					break;

				default:
					err = INPmkTemp("only mesfet device level 1-4 and hfet level 5-6 supported\n");
					break;
			}
			INPmakeMod(modname, type, image);
    }

    /*  --------  Check if model is a Uniform Distrib. RC line --------- */
    else if (strcmp(type_name, "urc") == 0) {
			type = INPtypelook("URC");
			if (type < 0) {
			    err =
				INPmkTemp
				("Device type URC not available in this binary\n");
			}
			INPmakeMod(modname, type, image);
    }

    /*  ------  Check if model is a VDMOS FET ------- */
    else if ((strcmp(type_name, "vdmos") == 0) ||
             (strcmp(type_name, "vdmosn") == 0) ||
             (strcmp(type_name, "vdmosp") == 0)) {
        type = INPtypelook("VDMOS");
        if (type < 0) {
            err =
                INPmkTemp
                ("Device type VDMOS not available in this binary\n");
        }
        INPmakeMod(modname, type, image);
    }

    /*  --------  Check if model is a MOSFET --------- */
    else if ((strcmp(type_name, "nmos") == 0)
	       || (strcmp(type_name, "pmos") == 0)
	       || (strcmp(type_name, "nsoi") == 0)
	       || (strcmp(type_name, "psoi") == 0)) {
			err = INPfindLev(line, &lev);
			switch (lev) {
			case 0:
			case 1:
			    type = INPtypelook("Mos1");
			    if (type < 0) {
				err =
				    INPmkTemp
				    ("Device type MOS1 not available in this binary\n");
			    }
			    break;
			case 2:
			    type = INPtypelook("Mos2");
			    if (type < 0) {
				err =
				    INPmkTemp
				    ("Device type MOS2 not available in this binary\n");
			    }
			    break;
			case 3:
			    type = INPtypelook("Mos3");
			    if (type < 0) {
				err =
				    INPmkTemp
				    ("Device type MOS3 not available in this binary\n");
			    }
			    break;
			case 4:
			    type = INPtypelook("BSIM1");
			    if (type < 0) {
				err =
				    INPmkTemp
				    ("Device type BSIM1 not available in this binary\n");
			    }
			    break;
			case 5:
			    type = INPtypelook("BSIM2");
			    if (type < 0) {
				err =
				    INPmkTemp
				    ("Device type BSIM2 not available in this binary\n");
			    }
			    break;
			case 6:
			    type = INPtypelook("Mos6");
			    if (type < 0) {
				err =
				    INPmkTemp
				    ("Device type MOS6 not available in this binary\n");
			    }
			    break;
			case 7:
			    type = INPtypelook("MOS7");
			    if (type < 0) {
				err =
				    INPmkTemp
				    ("Device type MOS7 not available in this binary\n");
			    }
			    break;
			case 8: case 49:
			    err = INPfindVer(line, ver);

			    if (prefix("3.0", ver)) {
			      type = INPtypelook("BSIM3v0");
			    }
			    if (prefix("3.1", ver)) {
			      type = INPtypelook("BSIM3v1");
			    }
			    if (prefix("3.2", ver)) { /* version string ver has to start with 3.2 */
			      type = INPtypelook("BSIM3v32");
			    }
			    if ( (strstr(ver, "default")) || (prefix("3.3", ver)) ) {
			      type = INPtypelook("BSIM3");
			    }
			    if (type < 0) {
			       err = tprintf("Device type BSIM3 version %s not available in this binary\n", ver);
			    }
			    break;
			case  9:
			    type = INPtypelook("Mos9");
			    if(type < 0) {
				    err = INPmkTemp
				    ("Device type MOS9 not available in this binary\n");
			    }
			    break;
			case 14: case 54:
			    err = INPfindVer(line, ver); /* mapping of minor versions >= 4.2.1 are included */
			    if ((prefix("4.0", ver)) || (prefix("4.1", ver)) || (prefix("4.2", ver)) || (prefix("4.3", ver)) || (prefix("4.4", ver)) || (prefix("4.5", ver))) {
			      type = INPtypelook("BSIM4v5");
			    }
			    if (prefix("4.6", ver)) {
			      type = INPtypelook("BSIM4v6");
			    }
			    if (prefix("4.7", ver)) {
			      type = INPtypelook("BSIM4v7");
			    }
			    if ( (strstr(ver, "default")) || (prefix("4.8", ver)) ) {
			      type = INPtypelook("BSIM4");
			    }
			    if (type < 0) {
			       err = tprintf("Device type BSIM4 version %s not available in this binary\n", ver);
			    }
			    break;
			case 15:
			    type = INPtypelook("BSIM5");
			    if (type < 0) {
				    err =
				    INPmkTemp
				    ("Device type BSIM5 not available in this binary\n");
			    }
			    break;
			case 55:
			    type = INPtypelook("B3SOIFD");
			    if (type < 0) {
				    err =
				    INPmkTemp
				    ("Device type B3SOIFD not available in this binary\n");
			    }
			    break;
			case 56:
			    type = INPtypelook("B3SOIDD");
			    if (type < 0) {
				    err =
				    INPmkTemp
				    ("Device type B3SOIDD not available in this binary\n");
			    }
			    break;
			case 57:
			    type = INPtypelook("B3SOIPD");
			    if (type < 0) {
				    err =
				    INPmkTemp
				    ("Device type B3SOIPD not available in this binary\n");
			    }
			    break;
			case 10: case 58:
			    type = INPtypelook("B4SOI");
			    if (type < 0) {
				    err =
				    INPmkTemp
				    ("Device type B4SOI V4.0 not available in this binary\n");
			    }
			    break;
			case 60:
			    type = INPtypelook("SOI3");
			    if (type < 0) {
				    err =
				    INPmkTemp
				    ("Device type SOI3 not available in this binary (STAG release)\n");
			    }
			    break;
			case 68:
			    type = INPtypelook("HiSIM2");
			    if (type < 0) {
				    err =
				    INPmkTemp
				    ("Device type HiSIM2 not available in this binary\n");
			    }
			    break;
			case 73:
			    err = INPfindVer(line, ver); /* mapping of minor versions >= 1.1 are included */
			    if ((prefix("1.1", ver)) || (prefix("1.2", ver))) {
			      type = INPtypelook("HiSIMHV1");
			    }
			    if ( (strstr(ver, "default")) || (prefix("2.0", ver)) || (prefix("2.1", ver)) || (prefix("2.2", ver)) ) {
			      type = INPtypelook("HiSIMHV2");
			    }
			    if (type < 0) {
			       err = tprintf("Device type HiSIMHV version %s not available in this binary\n", ver);
			    }
			    break;
			default:		/* placeholder; use level xxx for the next model */
			    err = find_osdi_type(type_name, lev, line, &type,
				"Only MOS device levels 1-6,8-10,14,49,54-58,60,68,73 and levels bound with osdi_bind are supported in this binary\n");
			    break;
			}
			INPmakeMod(modname, type, image);
    }
#ifdef  NDEV
    /*  --------  Check if model is a numerical device --------- */
    else if (strcmp(type_name, "ndev") == 0) {
			type = INPtypelook("NDEV");
			if (type < 0) {
			    err =
				INPmkTemp
				("Device type NDEV not available in this binary\n");
			}
			INPmakeMod(modname, type, image);
    }
#endif
    /*  --------  Check if model is a resistor --------- */
    else if (strcmp(type_name, "r") == 0) {
			err = INPfindLev(line,&lev);
			switch(lev) {
				case 0:
				case 1:
				default:
					type = find_loaded_osdi_type(type_name, lev, line);
					if (type < 0)
						type = INPtypelook("Resistor");
					if (type < 0) {
						err =
						INPmkTemp
						("Device type Resistor not available in this binary\n");
					}
				break;
			}
			INPmakeMod(modname, type, image);
    }

    /*  --------  Check if model is a PSPICE resistor --------- */
    else if (strcmp(type_name, "res") == 0) {
        type = INPtypelook("Resistor");
        if (type < 0) {
            err =
                INPmkTemp
                ("Device type Resistor not available in this binary\n");
        }
        INPmakeMod(modname, type, image);
    }

    /*  --------  Check if model is a transmission line of some sort --------- */
    else if(strcmp(type_name,"txl") == 0) {
      INPgetTok(&line,&val,1);
      while (*line != '\0') {
				if (*val == 'R' || *val == 'r') {
				  INPgetTok(&line,&val,1);
				  rval = INPevaluate(&val, &error1, 1);
				}
				if ((strcmp(val,"L") == 0)  || (strcmp(val,"l") == 0)) {
				  INPgetTok(&line,&val,1);
				  lval = INPevaluate(&val, &error1, 1);
				}
				INPgetTok(&line,&val,1);
			}
		  if(lval)
			  rval = rval/lval;
		  if (rval > 1.6e10) {
			  type = INPtypelook("TransLine");
			  INPmakeMod(modname,type,image);
		  }
		  if (rval > 1.6e9) {
			  type = INPtypelook("CplLines");
			  INPmakeMod(modname,type,image);
		  }
		  else {
			  type = INPtypelook("TransLine");
			  INPmakeMod(modname,type,image);
		  }
		  if(type < 0) {
			  err = INPmkTemp(
					"Device type TransLine not available in this binary\n");
      }
    }

    /*  --------  Check if model is a coupled transmission line --------- */
    else if(strcmp(type_name,"cpl") == 0) {
      type = INPtypelook("CplLines");
      if(type < 0) {
				err = INPmkTemp(
						"Device type CplLines not available in this binary\n");
			}
      INPmakeMod(modname,type,image);

    }

    /*  --------  Check if model is a cap --------- */
    else if (strcmp(type_name, "c") == 0) {
			type = INPtypelook("Capacitor");
			if (type < 0) {
			    err =
				INPmkTemp
				("Device type Capacitor not available in this binary\n");
			}
			INPmakeMod(modname, type, image);
    }

    /*  --------  Check if model is an ind --------- */
    else if (strcmp(type_name, "l") == 0) {
			type = INPtypelook("Inductor");
			if (type < 0) {
			    err =
				INPmkTemp
				("Device type Inductor not available in this binary\n");
			}
			INPmakeMod(modname, type, image);
    }


    /*  --------  Check if model is a switch --------- */
    else if (strcmp(type_name, "sw") == 0) {
			type = INPtypelook("Switch");
			if (type < 0) {
			    err =
				INPmkTemp
				("Device type Switch not available in this binary\n");
			}
			INPmakeMod(modname, type, image);
    }

    /*  --------  Check if model is a Current Controlled Switch --------- */
    else if (strcmp(type_name, "csw") == 0) {
			type = INPtypelook("CSwitch");
			if (type < 0) {
			    err =
				INPmkTemp
				("Device type CSwitch not available in this binary\n");
			}
			INPmakeMod(modname, type, image);
    }

    /*  --------  Check if model is a Lossy TransLine --------- */
    else if (strcmp(type_name, "ltra") == 0) {
			type = INPtypelook("LTRA");
			if (type < 0) {
			    err =
				INPmkTemp
				("Device type LTRA not available in this binary\n");
			}
			INPmakeMod(modname, type, image);
    }

#ifdef CIDER
    else if(strcmp(type_name,"numd") == 0) {
			 err = INPfindLev(line,&lev);
		    switch( lev ) {
		    case 1:
		    default:
					type = INPtypelook("NUMD");
					if(type < 0) {
					   err =
					   INPmkTemp
					   ("Device type NUMD not available in this binary\n");
					 }
					break;
		    case 2:
					type = INPtypelook("NUMD2");
					if(type < 0) {
					   err =
					   INPmkTemp
					   ("Device type NUMD2 not available in this binary\n");
					}
					break;
			}
	    INPmakeMod(modname,type,image);
    } else if(strcmp(type_name,"nbjt") == 0) {
			err = INPfindLev(line,&lev);
		    switch( lev ) {
		    case 1:
		    default:
					type = INPtypelook("NBJT");
					if(type < 0) {
					   err =
					   INPmkTemp
					   ("Device type NBJT not available in this binary\n");
					}
					break;
		    case 2:
					type = INPtypelook("NBJT2");
					if(type < 0) {
					   err =
					   INPmkTemp
					   ("Device type NBJT2 not available in this binary\n");
					}
					break;
		    }
			INPmakeMod(modname,type,image);
    } else if(strcmp(type_name,"numos") == 0) {
			type = INPtypelook("NUMOS");
			if(type < 0) {
			    err =
			    INPmkTemp
			    ("Device type NUMOS not available in this binary\n");
			 }
			INPmakeMod(modname,type,image);
    }
#endif /* CIDER */

    /*  type poly added by SDB  . . . */
#ifdef XSPICE
    /*  --------  Check if model is a poly (specific to xspice) --------- */
    else if ( strcmp(type_name, "poly") == 0 ||
	      strcmp(type_name, "POLY") == 0 ) {
	type = INPtypelook("POLY");
	if (type < 0) {
	    err =
		INPmkTemp
		("Device type POLY not available in this binary\n");
	}
	INPmakeMod(modname, type, image);
    }
#endif

    /*  --------  Default action  --------- */
    else {
#if !defined(XSPICE) && !defined(OSDI) 
	type = -1;
	err = tprintf("unknown model type %s - ignored\n", type_name);
#else
      /* gtri - modify - wbk - 10/23/90 - modify to look for code models */

#ifdef TRACE
      printf("In INPdomodel, found unknown model type, typename = %s . . .\n", type_name);
#endif

      /* look for this model type and put it in the table of models */
      type = INPtypelook(type_name);
      if(type < 0) {
	err = tprintf("Unknown model type %s - ignored\n", type_name);

#ifdef TRACE
	printf("In INPdomodel, ignoring unknown model typ typename = %s . . .\n", type_name);
#endif

      }
      else {

#ifdef TRACE
	printf("In INPdomodel, adding unknown model typename = %s to model list. . .\n", type_name);
#endif

	INPmakeMod(modname,type,image);
      }

      /* gtri - end - wbk - 10/23/90 */
#endif

    }
    tfree(type_name);
    return (err);
}
