#include "ngspice/ngspice.h" /* for wl */
#include "ngspice/ftedefs.h"
#include "ngspice/devdefs.h" /* solve deps in dev.h*/
#include "../spicelib/devices/dev.h" /* for load library commands */
#include "ngspice/inpdefs.h"
#include "com_dl.h"


#ifdef XSPICE
void com_codemodel(wordlist *wl)
{
if (wl && wl->wl_word)
#ifdef CM_TRACE
    fprintf(stdout, "Note: loading codemodel %s\n", ww->wl_word);
#endif
    if (load_opus(wl->wl_word)) {
        fprintf(stderr, "Error: Library %s couldn't be loaded!\n", wl->wl_word);
        ft_spiniterror = TRUE;
        ft_codemodelerror = copy(wl->wl_word);
        if (ft_stricterror) /* if set in spinit */
            controlled_exit(EXIT_BAD);
    }
#ifdef CM_TRACE
    else {
        fprintf(stdout, "Codemodel %s is loaded\n", wl->wl_word);
    }
#endif
}
#endif

#ifdef OSDI
void com_osdi(wordlist *wl)
{
    for (wordlist *word = wl; word; word = word->wl_next) {
        const char *path = word->wl_word;
        const char *version = NULL;
        const wordlist *next = word->wl_next;
        if (next && next->wl_next && cieq(next->wl_word, "version")) {
            word = next->wl_next;
            version = word->wl_word;
        }
        if (load_osdi(path, version)) {
            fprintf(cp_err, "Error: Library %s couldn't be loaded!\n", path);
            ft_spiniterror = TRUE;
            ft_osdierror = TRUE;
            if (ft_stricterror)
                controlled_exit(EXIT_BAD);
         }
    }
}

void com_osdi_bind(wordlist *wl)
{
    char *words[7];
    int word_count = 0;
    for (; wl; wl = wl->wl_next)
        words[word_count++] = wl->wl_word;
    const bool has_version = word_count >= 5 && cieq(words[word_count - 2], "version");
    const int word_count_without_version = word_count - (has_version ? 2 : 0);
    const bool has_polarity = word_count_without_version == 5;
    int level;
    if ((word_count_without_version != 3 && !has_polarity) || sscanf(words[1], "%d", &level) != 1) {
        fprintf(cp_err, "Error: osdi_bind needs model_type level module [parameter value] [version version]\n");
        return;
    }
    const char *type_name = words[0];
    const char *module_name = words[2];
    const char *polarity_parameter = has_polarity ? words[3] : NULL;
    const char *polarity_value = has_polarity ? words[4] : NULL;
    const char *version = has_version ? words[word_count - 1] : NULL;
    INPosdiBind(type_name, level, module_name, polarity_parameter, polarity_value, version);
}
#endif




#ifdef DEVLIB
void com_use(wordlist *wl)
{
    wordlist *ww;
    for (ww = wl; ww; ww = ww->wl_next)
        if (load_dev(wl->wl_word))
            fprintf(cp_err, "Error: Library %s couldn't be loaded!\n", ww->wl_word);
}
#endif
