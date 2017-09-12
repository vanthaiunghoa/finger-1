/*******************************************************************************

License: 
This software and/or related materials was developed at the National Institute
of Standards and Technology (NIST) by employees of the Federal Government
in the course of their official duties. Pursuant to title 17 Section 105
of the United States Code, this software is not subject to copyright
protection and is in the public domain. 

This software and/or related materials have been determined to be not subject
to the EAR (see Part 734.3 of the EAR for exact details) because it is
a publicly available technology and software, and is freely distributed
to any interested party with no licensing requirements.  Therefore, it is 
permissible to distribute this software as a free download from the internet.

Disclaimer: 
This software and/or related materials was developed to promote biometric
standards and biometric technology testing for the Federal Government
in accordance with the USA PATRIOT Act and the Enhanced Border Security
and Visa Entry Reform Act. Specific hardware and software products identified
in this software were used in order to perform the software development.
In no case does such identification imply recommendation or endorsement
by the National Institute of Standards and Technology, nor does it imply that
the products and equipment identified are necessarily the best available
for the purpose.

This software and/or related materials are provided "AS-IS" without warranty
of any kind including NO WARRANTY OF PERFORMANCE, MERCHANTABILITY,
NO WARRANTY OF NON-INFRINGEMENT OF ANY 3RD PARTY INTELLECTUAL PROPERTY
or FITNESS FOR A PARTICULAR PURPOSE or for any purpose whatsoever, for the
licensed product, however used. In no event shall NIST be liable for any
damages and/or costs, including but not limited to incidental or consequential
damages of any kind, including economic damage or injury to property and lost
profits, regardless of whether NIST shall be advised, have reason to know,
or in fact shall know of the possibility.

By using this software, you agree to bear all risk relating to quality,
use and performance of the software and/or related materials.  You agree
to hold the Government harmless from any claim arising from your use
of the software.

*******************************************************************************/

/******************************************************************************

      PACKAGE: ANSI/NIST 2007 Standard Reference Implementation

      FILE:    CHKFILE.C

      AUTHOR:  Bruce Bandini

      DATE:    01/18/2010

#cat: chkfile - Parses the text file of an ANSI/NIST 2007 file that is
#cat            generated by an2k2txt and builds a histogram of Field Numbers (FN).
#cat            For example of Field Number, see document NIST Special Publi-
#cat            cation 500-271 ANSI/NIST-ITL 1-2007, Table 8 on page 24.
#cat            The text file containing the metadata is read line by line
#cat            until EOF.  The FN is parsed and stored in a linked list.
#cat            If the FN does not yet exist in the linked list, it is added
#cat            and its count is set to 1.  For each subsequent "find"
#cat            of an existing FN, its count is incremented by 1.
#cat
#cat            According to Publication 500-271 ANSI/NIST-ITL 1-2007, the Type-1
#cat            transaction information record is mandatory.  The first FN is
#cat            also mandatory.  Therefore, this application shall assume that
#cat            the first FN = 1.001 in all an2k2txt files.  The consequence of
#cat            this assumption is that any text file that starts with or
#cat            contains a FN < 1.001 shall be determined as invalid and this
#cat            application shall exit.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "histogen.h"

extern char *basename(const char *);
extern FILE *fp_histo_log;
extern unsigned gl_num_paths;
extern unsigned gl_total_field_nums;
extern unsigned gl_num_valid;
extern unsigned gl_num_invalid_ansi_nist;
extern unsigned gl_num_invalid_ext;
extern short gl_opt_flags[];
extern char *gl_nbis_bin_dir;
extern char *tmp_path;

int  process_file(const char *);
void process_an2(const char *, const char *);
void process_an2k2txt_txtfile(const char *);
void copy_to_temp(const char *);
int  initialize_linked_list();
int  update_linked_list();
int  output_linked_list(FILE *);

unsigned char c; /* used to convert ext to upper case */
char *org;       /* used to store original pointer */
char *ext;       /* used to store filename extension */
char *filename;
int  slen;
char cmd[CMD_LEN];
const unsigned separators[] = { 28, 29, 30, 31 };

FILE *filein;
FILE *from, *to;

char gl_field_newline[MAX_FIELD_NUMS][MAX_FIELD_NUM_CHARS];
int  idx_newline;
char gl_field_space[MAX_FIELD_NUMS][MAX_FIELD_NUM_CHARS];
int  idx_space;
char gl_field_sep[MAX_FIELD_NUMS][MAX_FIELD_NUM_CHARS];
int  idx_sep;
char *files_invalid_ext;
char *files_invalid_ansi_nist;

/* linked list support variables */
unsigned node_count;

/******************************************************************************/
int process_file(const char *filepath)
{
  char ext_org[80];
  filename = basename(filepath);

  /* strip trailing '\n' if it exists */
  slen = strlen(filename)-1;
  if(filename[slen] == '\n')
    filename[slen] = '\0';

  ext = strrchr(filepath, '.');
  ext++;
  strcpy(ext_org, ext);

  /* save pointer to ext location */
  org = ext;

  /* convert ext to upper, note that pointer to ext is modified */
  while(*ext) {
    c = *ext;
    *ext = toupper(c);
    ext++;
  }

  /* restore pointer to ext location */
  ext = org;
  if ((str_eq("AN2",ext)) || (str_eq("SUB",ext))) {
    /* restore the original extension before copy to temp dir */
    strcpy(org, ext_org);
    copy_to_temp(filepath);
    process_an2(filepath, filename);
  }
  else {  /* not a valid file extension */
    strcpy(org, ext_org);
    if((gl_opt_flags[0] == INCLUDE_INVALID_FILES) &&
       (strcmp(filename, HISTOGEN_LOG_FNAME))) {
      gl_num_invalid_ext++;

      if (files_invalid_ext == NULL) {
	files_invalid_ext = realloc(NULL, strlen(filepath)+3);
	strcpy(files_invalid_ext, filepath);
	strcat(files_invalid_ext, "\n");
      }
      else {
	files_invalid_ext = realloc(files_invalid_ext, (strlen(files_invalid_ext) + strlen(filepath)+3));
	strcat(files_invalid_ext, filepath);
	strcat(files_invalid_ext, "\n");
      }
    }
  }

  return 0;
}

/*****************************************************************************/
void process_an2(const char *filepath, const char *filename)
{
  char txtfile[180];
  int status = 0;

  sprintf(txtfile, "%s%s.txt", tmp_path, filename);
  sprintf(cmd, "an2k2txt \"%s\" \"%s\" > log.log 2>&1", filepath, txtfile);
  system(cmd);

  /* open an2k txt file, build the histogram */
  if((filein=fopen(txtfile,"r"))!=NULL)
  {
    process_an2k2txt_txtfile(filepath);
    gl_num_valid++;
    fclose(filein);
  }
  else
  {  /* an2/sub file could not be processed by an2k2txt */
    if(gl_opt_flags[0] == INCLUDE_INVALID_FILES) {
      gl_num_invalid_ansi_nist++;

      if (files_invalid_ansi_nist == NULL) {
	files_invalid_ansi_nist = realloc(NULL, strlen(filepath)+3);
	strcpy(files_invalid_ansi_nist, filepath);
	strcat(files_invalid_ansi_nist, "\n");
      }
      else {
	files_invalid_ansi_nist = realloc(files_invalid_ansi_nist,
                                         (strlen(files_invalid_ansi_nist) + strlen(filepath)+3));
	strcat(files_invalid_ansi_nist, filepath);
	strcat(files_invalid_ansi_nist, "\n");
      }
    }
    status = -1;
  }

  /* processing complete, perform cleanup */
  sprintf(cmd, "%s%s", tmp_path, filename);
  remove(cmd);
  strcat(cmd, ".txt");
  remove(cmd);
  sprintf(cmd, "log.log");
  remove(cmd);

  /* do not attempt deletion if an2k2txt failed */
  if (status != -1) {

#ifdef __MSYS__
    sprintf(cmd, "del *.tmp");
#else
    sprintf(cmd, "rm *.tmp");
#endif
    system(cmd);
  }
}

/*****************************************************************************/
void process_an2k2txt_txtfile(const char *filepath)
{
  int  i, k, slen;
  char line[60];
  char line_copy[60];
  char c, temp[32];
  char *ptemp  = &temp[0];
  char *start;

  idx_newline = 0;
  idx_space = 0;
  idx_sep = 0;

  while (fgets (line, sizeof line, filein) != NULL) {

    /* strip trailing '\n' if it exists */
    slen = strlen(line)-1;
    if(line[slen] == '\n')
      line[slen] = '\0';

    strcpy(line_copy, line);
    ptemp = strrchr(line, '[');

    /* if '[' char not found, continue to the next line */
    if(NULL == ptemp) {
      printf("Found NULL instead of '['\n");
      break;
    }

    /* ok to update the histogram */
    start = ptemp;
    start++;

    /* look for the ending ']' */
    while(*ptemp) {
      c = *ptemp;

      if(']' == *ptemp) {
        *ptemp = '\0';
	break;
      }
      ptemp++;
    }

    update_linked_list(start);

    /* now check for invalid chars after the '=' on the line */
    ptemp = strrchr(line_copy, '=');

    /* if '=' char not found, continue to the next line */
    if(NULL == ptemp) {
      fprintf(fp_histo_log, "Found an2k2txt file containing line without '='\n");
      continue;
    }
    ptemp++;

    /* check for separator char immediately following '=' */
    for(k=0; k<4; k++) {
      if(*ptemp==separators[k]) {
        strcpy(gl_field_sep[idx_sep], start);
	idx_sep++;
      }
    }

    /* check each char after the '=', one at a time */
    while (*ptemp != '\0') {
      c = *ptemp;
      ptemp++;

      if((c==0x0a) || (c==0x0d)) {
        strcpy(gl_field_newline[idx_newline], start);
	idx_newline++;
      }

      if(c==0x20) {
        strcpy(gl_field_space[idx_space], start);
	idx_space++;
      }
    }
  }

  /* write the processed filename to the histo.log file */
  fprintf(fp_histo_log, "%s\n", filepath);

  if((idx_sep > 0) && (gl_opt_flags[1] == INCLUDE_FIELD_SEPARATORS)) {
    fprintf(fp_histo_log, "separation char immediately after = :\n");
    for(i=0; i<idx_sep; i++)
      fprintf(fp_histo_log, "%s ", gl_field_sep[i]);

    fprintf(fp_histo_log, "\n");
  }

  if((idx_newline > 0) && (gl_opt_flags[2] == INCLUDE_NEWLINE_CHARS))  {
    fprintf(fp_histo_log, "newline before sep char:\n");
    for(i=0; i<idx_newline; i++)
      fprintf(fp_histo_log, "%s ", gl_field_newline[i]);

    fprintf(fp_histo_log, "\n");
  }

  if((idx_space > 0) && (gl_opt_flags[3] == INCLUDE_SPACE_CHARS)) {
    fprintf(fp_histo_log, "space between = and sep char:\n");
    for(i=0; i<idx_space; i++)
      fprintf(fp_histo_log, "%s ", gl_field_space[i]);

    fprintf(fp_histo_log, "\n");
  }

  fprintf(fp_histo_log, "\n");
}

/*****************************************************************************/
void copy_to_temp(const char *filepath) {

  char ch;
  char temp[255];
  unsigned short file_ok = TRUE;

  strcpy(temp, tmp_path);

  /* open source file */
  if((from = fopen(filepath, "rb"))==NULL) {
    printf("Cannot open source file:'%s'\n", filepath);
    file_ok = FALSE;
  }
  strcat(temp, basename(filepath));

  /* open destination file */
  if((to = fopen(temp, "wb"))==NULL) {
    printf("Cannot open dest file:'%s'\n", temp);
    file_ok = FALSE;
  }

  /* copy the file */
  if (file_ok) {
    while(!feof(from)) {
      ch = fgetc(from);
      if(ferror(from)) {
	printf("Error reading source file:'%s'\n", filepath);
	break;
      }

      if(!feof(from)) fputc(ch, to);

      if(ferror(to)) {
	printf("Error writing destination file:'%s'\n", temp);
	break;
      }
    }

    fclose(from);
    fclose(to);
  }
}

/*****************************************************************************/
int initialize_linked_list() {

  histo_head = NULL;
  node_count = 0;

  return 0;
}

/*****************************************************************************/
int update_linked_list(const char *fn) {

  HISTO *new_node = NULL;
  HISTO *prev_node = NULL;
  HISTO *curr_node = histo_head;
  char *end;
  double dfield_num;
  double dfn = strtod(fn, &end);

  /* linked list is empty */
  if (curr_node == NULL) {
    /* INSERT start */
    new_node = malloc(sizeof(HISTO));
    if (new_node == NULL) {
      printf("malloc error for new node linked list\n");
      return -1;
    }
    node_count++;

    strcpy(new_node->field_num, fn);
    new_node->count = 1;

    /* INSERT end */
    new_node->next = curr_node; /* is NULL */
    histo_head = new_node;
    return 0;
  }

  /* linked list contains at least one node */
  while (curr_node != NULL) {
    /* convert string to int for comparison */
    dfield_num = strtod(curr_node->field_num, &end);

    /* check if field num is already in linked list */
    if (dfn == dfield_num) {
      curr_node->count++;
      break;
    }

    /* start check for insertion into linked list */
    if (dfn < dfield_num) {
      /* INSERT start */
      new_node = malloc(sizeof(HISTO));
      if (new_node == NULL) {
	printf("malloc error for new node linked list\n");
	return -1;
      }
      node_count++;

      strcpy(new_node->field_num, fn);
      new_node->count = 1;

      /* INSERT end */
      prev_node->next = new_node;
      new_node->next = curr_node;
      break;
    }
    else {
      /* check if last node, that is, this is a new field num */
      if (curr_node->next == NULL) {
	/* insert last node */

	/* INSERT start */
	new_node = malloc(sizeof(HISTO));
	if (new_node == NULL) {
	  printf("malloc error for new node linked list\n");
	  return -1;
	}
	node_count++;

	strcpy(new_node->field_num, fn);
	new_node->count = 1;

	/* INSERT end */
	curr_node->next = new_node;
	new_node->next = NULL;
	break;
      }
    }
    prev_node = curr_node;
    curr_node = curr_node->next;
  }

  return 0;
}

/*****************************************************************************/
int output_linked_list(FILE *fp_histo_log) {

  HISTO *curr_node = histo_head;

  if(gl_opt_flags[0] == INCLUDE_INVALID_FILES) {
    fprintf(fp_histo_log, "INVALID ANSI/NIST file(s):\n");
    if (files_invalid_ansi_nist != NULL) {
      fprintf(fp_histo_log, "%s\n\n", files_invalid_ansi_nist);
    }
    else {
      fprintf(fp_histo_log, "NONE\n\n");
    }

    fprintf(fp_histo_log, "Files with INVALID extension(s):\n");

    if (files_invalid_ext != NULL) {
      fprintf(fp_histo_log, "%s\n\n", files_invalid_ext);
    }
    else {
      fprintf(fp_histo_log, "NONE\n\n");
    }
  }

  while (curr_node != NULL) {
    fprintf(fp_histo_log, "[%s]:%d\n", curr_node->field_num, curr_node->count);
    curr_node = curr_node->next;
  }

  free(files_invalid_ext);
  free(files_invalid_ansi_nist);
  return 0;
}


