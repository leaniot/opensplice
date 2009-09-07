/*
 *                         OpenSplice DDS
 *
 *   This software and documentation are Copyright 2006 to 2009 PrismTech
 *   Limited and its licensees. All rights reserved. See file:
 *
 *                     $OSPL_HOME/LICENSE
 *
 *   for full copyright notice and license terms.
 *
 */
#include "idl_program.h"
#include "idl_scope.h"
#include "idl_genSplHelper.h"
#include "idl_tmplExp.h"
#include "idl_keyDef.h"
#include "idl_dll.h"

#include "idl_genSACSTypedClassDefs.h"
#include "idl_genSACSHelper.h"

#include "os_heap.h"
#include "os_stdlib.h"
#include "c_typebase.h"

static idl_macroAttrib idlpp_macroAttrib;
static idl_streamIn idlpp_inStream;
static c_char *idlpp_template;
static c_char *idlpp_tmplPrefix;
static idl_macroSet idlpp_macroSet;
static c_long idlpp_indent_level = 0;

/* QAC EXPECT 0285; Need dollar here, this is specified */
#define IDL_TOKEN_START     '$'
#define IDL_TOKEN_OPEN      '('
#define IDL_TOKEN_CLOSE     ')'

static idl_action
idl_fileOpen(
    idl_scope scope,
    const char *name,
    void *userData)
{
    idl_tmplExp te;
    c_char tmplFileName[1024];
    c_char *tmplPath;
    c_char *orbPath;
    int tmplFile;
    struct os_stat tmplStat;
    unsigned int nRead;

    tmplPath = os_getenv("OSPL_TMPL_PATH");
    orbPath = os_getenv("OSPL_ORB_PATH");
    if (tmplPath == NULL) {
        printf ("OSPL_TMPL_PATH not defined\n");
        return (idl_abort);
    }
    if (orbPath == NULL) {
        printf ("OSPL_ORB_PATH not defined\n");
        return (idl_abort);
    }

    /* Prepare file header template */
    snprintf(tmplFileName, (size_t)sizeof(tmplFileName), "%s%c%s%c%sHeader", tmplPath, OS_FILESEPCHAR, orbPath, OS_FILESEPCHAR, idlpp_tmplPrefix);
    /* QAC EXPECT 3416; No side effects here */
    if ((os_stat(tmplFileName, &tmplStat) != os_resultSuccess) ||
        (os_access(tmplFileName, OS_ROK) != os_resultSuccess)) {
        printf ("No template found or protection violation (%s)\n", tmplFileName);
        return (idl_abort);
    }
    /* QAC EXPECT 5007; will not use wrapper */
    idlpp_template = os_malloc((size_t)((int)tmplStat.stat_size+1));
    tmplFile = open(tmplFileName, O_RDONLY);
    nRead = (unsigned int)read(tmplFile, idlpp_template, (size_t)tmplStat.stat_size);
    memset(&idlpp_template[nRead], 0, (size_t)((int)tmplStat.stat_size+1-nRead));
    close(tmplFile);
    idlpp_macroAttrib = idl_macroAttribNew(IDL_TOKEN_START, IDL_TOKEN_OPEN, IDL_TOKEN_CLOSE);
    idlpp_macroSet = idl_macroSetNew();
    idlpp_inStream = idl_streamInNew(idlpp_template, idlpp_macroAttrib);
    /* Expand file header */
    idl_macroSetAdd(idlpp_macroSet, idl_macroNew("basename", name));

    te = idl_tmplExpNew(idlpp_macroSet);
    idl_tmplExpProcessTmpl(te, idlpp_inStream, idl_fileCur());
    idl_streamInFree(idlpp_inStream);
    idl_tmplExpFree(te);

    /* Prepare class definition template */
    snprintf(tmplFileName, (size_t)sizeof(tmplFileName), "%s%c%s%c%s", tmplPath, OS_FILESEPCHAR, orbPath, OS_FILESEPCHAR, idlpp_tmplPrefix);
    /* QAC EXPECT 3416; No side effects here */
    if ((os_stat(tmplFileName, &tmplStat) != os_resultSuccess) ||
        (os_access(tmplFileName, OS_ROK) != os_resultSuccess)) {
        printf ("No template found or protection violation (%s)\n", tmplFileName);
        return (idl_abort);
    }
    /* QAC EXPECT 5007; will not use wrapper */
    idlpp_template = os_malloc((size_t)((int)tmplStat.stat_size+1));
    tmplFile = open(tmplFileName, O_RDONLY);
    nRead = (unsigned int)read(tmplFile, idlpp_template, (size_t)tmplStat.stat_size);
    memset(&idlpp_template[nRead], 0, (size_t)((int)tmplStat.stat_size+1-nRead));
    close(tmplFile);

    if (userData)
    {
        /* Walk over the available meta-data and generate the appropriate typeDescriptors. */
        os_iter *metaList = (os_iter *) userData;
        os_iterWalk(*metaList, idl_metaCsharpSerialize2XML, NULL);
    }

    idlpp_indent_level = 0;

    return idl_explore;
    /* QAC EXPECT 2006; overview does not get better with one exit */
}

//static void
//idl_fileClose(
//    void *userData)
//{
//    idl_fileOutPrintf(idl_fileCur(), "#endif\n");
//}

static idl_action
idl_moduleOpen(
    idl_scope scope,
    const char *name,
    void *userData)
{
    idl_printIndent(idlpp_indent_level);
    idl_fileOutPrintf(idl_fileCur(), "namespace %s\n", idl_CsharpId(name));
    idl_printIndent(idlpp_indent_level);
    idl_fileOutPrintf(idl_fileCur(), "{\n");
    idlpp_indent_level++;
    return idl_explore;
}

static void
idl_moduleClose(
    void *userData)
{
    idlpp_indent_level--;
    idl_printIndent(idlpp_indent_level);
    idl_fileOutPrintf(idl_fileCur(), "}\n");
    idl_fileOutPrintf(idl_fileCur(), "\n");
}

static idl_action
idl_structureOpen(
    idl_scope scope,
    const char *name,
    idl_typeStruct structSpec,
    void *userData)
{
    c_char spaces[20];
    idl_tmplExp te;

    /* QAC EXPECT 3416; No side effects here */
    if (idl_keyResolve(idl_keyDefDefGet(), scope, name) != NULL) {
        /* keylist has been defined for this struct, so it acts as topic. */
        te = idl_tmplExpNew(idlpp_macroSet);
        idl_macroSetAdd(idlpp_macroSet,
            idl_macroNew("scope", idl_CsharpId(idl_scopeElementName(idl_scopeCur(scope)))));
        idl_macroSetAdd(idlpp_macroSet, idl_macroNew("typename", idl_CsharpId(name)));
        idl_macroSetAdd(idlpp_macroSet,
                idl_macroNew("scoped-meta-type-name", idl_scopeStack(scope, "::", name)));
        snprintf(spaces, (size_t)sizeof(spaces), "%d", idlpp_indent_level*4);
        idl_macroSetAdd(idlpp_macroSet, idl_macroNew("spaces", spaces));
        idl_macroSetAdd(idlpp_macroSet, idl_macroNew("key-list", idl_keyResolve(idl_keyDefDefGet(), scope, name)));

        if (userData)
        {
            os_iter *metaList = (os_iter *) userData;
            idl_metaCsharp *metaElmnt = os_iterTakeFirst(*metaList);
            idl_macroSetAdd(idlpp_macroSet, idl_macroNew("meta-descriptor", metaElmnt->descriptor));
        }

        idlpp_inStream = idl_streamInNew(idlpp_template, idlpp_macroAttrib);
        idl_tmplExpProcessTmpl(te, idlpp_inStream, idl_fileCur());
        idl_streamInFree(idlpp_inStream);
        idl_tmplExpFree(te);
    }
    return idl_abort;
}

static idl_action
idl_unionOpen(
    idl_scope scope,
    const char *name,
    idl_typeUnion unionSpec,
    void *userData)
{
    c_char spaces[20];
    idl_tmplExp te;

    /* QAC EXPECT 3416; No side effects here */
    if (idl_keyResolve(idl_keyDefDefGet(), scope, name) != NULL) {
    /* keylist defined for this union */
        te = idl_tmplExpNew(idlpp_macroSet);
        idl_macroSetAdd(idlpp_macroSet,
            idl_macroNew("scope", idl_CsharpId(idl_scopeElementName(idl_scopeCur(scope)))));
        idl_macroSetAdd(idlpp_macroSet, idl_macroNew("typename", idl_CsharpId(name)));
        snprintf(spaces, (size_t)sizeof(spaces), "%d", idlpp_indent_level*4);
        idl_macroSetAdd(idlpp_macroSet, idl_macroNew("spaces", spaces));
        idlpp_inStream = idl_streamInNew(idlpp_template, idlpp_macroAttrib);
        idl_tmplExpProcessTmpl(te, idlpp_inStream, idl_fileCur());
        idl_streamInFree(idlpp_inStream);
        idl_tmplExpFree(te);
    }
    return idl_abort;
}

//static void
//idl_typedefOpenClose(
//    idl_scope scope,
//    const char *name,
//    idl_typeDef defSpec,
//    void *userData)
//{
//    c_char spaces[20];
//    idl_tmplExp te;
//
//    if ((idl_typeSpecType(idl_typeDefRefered(defSpec)) == idl_tstruct ||
//        idl_typeSpecType(idl_typeDefRefered(defSpec)) == idl_tunion) &&
//        idl_keyResolve(idl_keyDefDefGet(), scope, name) != NULL) {
//        /* keylist defined for this typedef of struct or union */
//        te = idl_tmplExpNew(idlpp_macroSet);
//        idl_macroSetAdd(idlpp_macroSet,
//            idl_macroNew("scope", idl_cxxId(idl_scopeElementName(idl_scopeCur(scope)))));
//        idl_macroSetAdd(idlpp_macroSet, idl_macroNew("typename", idl_cxxId(name)));
//        snprintf(spaces, (size_t)sizeof(spaces), "%d", idlpp_indent_level*4);
//        idl_macroSetAdd(idlpp_macroSet, idl_macroNew("spaces", spaces));
//        idlpp_inStream = idl_streamInNew(idlpp_template, idlpp_macroAttrib);
//        idl_tmplExpProcessTmpl(te, idlpp_inStream, idl_fileCur());
//        idl_streamInFree(idlpp_inStream);
//        idl_tmplExpFree(te);
//    }
//}

static struct idl_program idl_genSACSTypedClassDefs;

idl_program
idl_genSACSTypedClassDefsProgram(c_char *tmplPrefix, os_iter *metaList)
{
    idlpp_tmplPrefix = tmplPrefix;

    idl_genSACSTypedClassDefs.idl_getControl                  = NULL;
    idl_genSACSTypedClassDefs.fileOpen                        = idl_fileOpen;
    idl_genSACSTypedClassDefs.fileClose                       = NULL;
    idl_genSACSTypedClassDefs.moduleOpen                      = idl_moduleOpen;
    idl_genSACSTypedClassDefs.moduleClose                     = idl_moduleClose;
    idl_genSACSTypedClassDefs.structureOpen                   = idl_structureOpen;
    idl_genSACSTypedClassDefs.structureClose                  = NULL;
    idl_genSACSTypedClassDefs.structureMemberOpenClose        = NULL;
    idl_genSACSTypedClassDefs.enumerationOpen                 = NULL;
    idl_genSACSTypedClassDefs.enumerationClose                = NULL;
    idl_genSACSTypedClassDefs.enumerationElementOpenClose     = NULL;
    idl_genSACSTypedClassDefs.unionOpen                       = idl_unionOpen;
    idl_genSACSTypedClassDefs.unionClose                      = NULL;
    idl_genSACSTypedClassDefs.unionCaseOpenClose              = NULL;
    idl_genSACSTypedClassDefs.unionLabelsOpenClose            = NULL;
    idl_genSACSTypedClassDefs.unionLabelOpenClose             = NULL;
    idl_genSACSTypedClassDefs.typedefOpenClose                = NULL;
    idl_genSACSTypedClassDefs.boundedStringOpenClose          = NULL;
    idl_genSACSTypedClassDefs.sequenceOpenClose               = NULL;
    idl_genSACSTypedClassDefs.constantOpenClose               = NULL;
    idl_genSACSTypedClassDefs.artificialDefaultLabelOpenClose = NULL;
    idl_genSACSTypedClassDefs.userData                        = metaList;

    return &idl_genSACSTypedClassDefs;
}