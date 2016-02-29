/* Copyright (c) 2016, hz86@live.cn. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include "mysqld_error.h"
#include <stdlib.h>
#include <ctype.h>
#include <mysql/plugin_ftparser.h>
#include <m_ctype.h>


/* atom-ngram token size, by default bigram. */
static int atom_ngram_token_size;


/*
  atom_ngram_parser interface functions:

  Plugin declaration functions:
  - atom_ngram_parser_plugin_init()
  - atom_ngram_parser_plugin_deinit()

  Parser descriptor functions:
  - atom_ngram_parser_parse()
  - atom_ngram_parser_init()
  - atom_ngram_parser_deinit()
*/


/*
  Initialize the parser plugin at server start or plugin installation.

  SYNOPSIS
    atom_ngram_parser_plugin_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int atom_ngram_parser_plugin_init(void *arg __attribute__((unused)))
{
  return(0);
}


/*
  Terminate the parser plugin at server shutdown or plugin deinstallation.

  SYNOPSIS
    atom_ngram_parser_plugin_deinit()
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)

*/

static int atom_ngram_parser_plugin_deinit(void *arg __attribute__((unused)))
{
  return(0);
}


/*
  Initialize the parser on the first use in the query

  SYNOPSIS
    atom_ngram_parser_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int atom_ngram_parser_init(MYSQL_FTPARSER_PARAM *param
                                  __attribute__((unused)))
{
  return(0);
}


/*
  Terminate the parser at the end of the query

  SYNOPSIS
    atom_ngram_parser_deinit()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int atom_ngram_parser_deinit(MYSQL_FTPARSER_PARAM *param
                                    __attribute__((unused)))
{
  return(0);
}


//atom-ngram �ص�����
typedef int(*ATOM_NGRAM_CALLBACK)(
  char* doc, int doc_len, char* word, int len, 
  MYSQL_FTPARSER_PARAM *param, MYSQL_FTPARSER_BOOLEAN_INFO *bool_info
);

//atom-ngram ������
static int atom_ngram_parser(
  char* doc, int doc_len, int token_size, 
  ATOM_NGRAM_CALLBACK callback, MYSQL_FTPARSER_PARAM *param,
  MYSQL_FTPARSER_BOOLEAN_INFO *bool_info)
{
  //abcd
  //n=1, a b c d
  //n=2, ab bc cd
  //n=3, abc bcd
  //n=4, abcd
  
  int ret = 0;
  char* start = doc;
  char* end = doc + doc_len;
  char* next = start;
  int nlen, slen,
    n = 0;
  
  while (next < end)
  {
    //�����ַ����ȣ���򳬳�������ѭ��
    nlen = my_mbcharlen_ptr(param->cs, next, end);
    if(0 == nlen || next + nlen > end) {
      break;
	}
    
    //��һ��������¼����
	if(0 == n) slen = nlen;
    next += nlen;
    n++;
    
    //�ʴ�С��������ֵ���ַ���β
    if (token_size == n || end == next)
    {
      //�Ѵʴ��ݸ��ص�����
      ret = callback(doc, doc_len,
        start, (int)(next - start), param, bool_info);
      
      //���ش���򵽴��β������
      if (0 != ret || end == next) {
        break;
      }
      
      //��ʼ��
      start += slen;
      next = start;
      n = 0;
    }
  }
  
  return(ret);
}

//atom-ngram ��ģʽ����
static int atom_ngram_simple_callback(
  char* doc, int doc_len, char* word, int len,
  MYSQL_FTPARSER_PARAM *param, MYSQL_FTPARSER_BOOLEAN_INFO *bool_info)
{
  bool_info->type = FT_TOKEN_WORD;
  bool_info->position = (int)(word - doc);
  return param->mysql_add_word(param, word, len, bool_info);
}

//�ַ��� \ ת�������
static char * atom_ngram_escape(
  char* str, int slen, int *rlen,
  MYSQL_FTPARSER_PARAM *param)
{
  char * nstr = (char *)malloc(slen);
  if (NULL != nstr)
  {
    char* start = str;
    char* end = str + slen;
    int nlen, ctype, escape = 0;
	*rlen = 0;

    while (start < end)
    {
      nlen = param->cs->cset->ctype(
        param->cs, &ctype, (uchar *)start, (uchar *)end);
      
      if (0 == nlen || 
        start + nlen > end) {
        break;
      }
      
      if (0 == escape)
      {
        if (0 != my_isascii(ctype))
        {
          if ('\\' == *start) {
            start += nlen;
            escape = 1;
            continue;
          }
        }
      }
      else
      {
		  escape = 0;
      }
      
      memcpy(nstr + *rlen, start, nlen);
      (*rlen) += nlen;
      start += nlen;
    }
  }
  
  return(nstr);
}

//atom-ngram boolean �ص�����
typedef int(*ATOM_NGRAM_BOOLEAN_CALLBACK)(
  char* doc, int doc_len, char* word, int len,
  MYSQL_FTPARSER_PARAM *param, MYSQL_FTPARSER_BOOLEAN_INFO *bool_info
);

//����ģʽ����
static int atom_ngram_boolean_parser(
  char* doc, int doc_len, ATOM_NGRAM_BOOLEAN_CALLBACK callback, 
  MYSQL_FTPARSER_PARAM *param, MYSQL_FTPARSER_BOOLEAN_INFO *bool_info)
{
  int ret = 0;
  char* start = doc;
  char* end = doc + doc_len;
  char* next = start;
  
  char chr;
  int nlen, wlen, ctype,
    escape = 0;
  
  //���ò���״̬
  bool_info->yesno = bool_info->weight_adjust =
    bool_info->wasign = bool_info->trunc = 0;
  
  //��ʼ�����﷨
  while (next < end)
  {
    //��ȡ�ַ����Ⱥ�����
    nlen = param->cs->cset->ctype(
      param->cs, &ctype, (uchar *)next, (uchar *)end);
    
    //����Ϊ��򳬳�������ѭ��
    if (0 == nlen || next + nlen > end) {
      break;
    }
    
    //�Ƿ�ת��
    if (0 == escape)
    {
      //ascii���������
      if (0 != my_isascii(ctype))
      {
        chr = *next;
        if ('\\' == chr)
        {
          //��⵽ת��
          //ת��״̬����
          escape = 1;
        }
        else if ('+' == chr || ' ' == chr || '-' == chr ||
          '>' == chr || '<' == chr || '~' == chr || '*' == chr ||
          '"' == chr || '(' == chr || ')' == chr)
        {
          //�ʽضϴ���
          //������״̬ �� ���������ַ��ڴ���
          if (('"' != chr && 0 == bool_info->quot) || '"' == chr)
          {
            //�ַ������ȷ�0
            if (0 != (wlen = (int)(next - start)))
            {
              //ͨ��������ŵĴ���
              if ('*' == chr) bool_info->trunc = 1;
              if (0 != bool_info->quot) bool_info->yesno = +1;
              
              //�Ѵʴ��ݸ��ص�����
              bool_info->type = FT_TOKEN_WORD;
              ret = callback(doc, doc_len, start, wlen, param, bool_info);
              if (0 != ret) break;
              
              //���ò���״̬
              bool_info->yesno = bool_info->weight_adjust =
                bool_info->wasign = bool_info->trunc = 0;
            }
          }
          
          //���ȴ�������
          if ('"' == chr || 0 != bool_info->quot)
          {
            //������������
            if ('"' == chr)
            {
              //��������ģʽ
              //�����ӱ��ʽ�ͱ�������ַ�
              
              bool_info->yesno = +1;
              bool_info->position = 0;
              bool_info->quot = (0 == bool_info->quot ? (char *)1 : 0);
              bool_info->type = (0 != bool_info->quot ? FT_TOKEN_LEFT_PAREN : FT_TOKEN_RIGHT_PAREN);
              ret = param->mysql_add_word(param, NULL, 0, bool_info);
              if (0 != ret) break;
              
              //���ò���״̬
              bool_info->yesno = bool_info->weight_adjust =
                bool_info->wasign = bool_info->trunc = 0;
              
              //�ַ�����ʼλ��
              start = next + nlen;
            }
          }
          else
          {
            //����ģʽ�﷨����
            if ('+' == chr) bool_info->yesno = +1;
            else if ('-' == chr) bool_info->yesno = -1;
            else if ('>' == chr) bool_info->weight_adjust++;
            else if ('<' == chr) bool_info->weight_adjust--;
            else if ('~' == chr) bool_info->wasign = !bool_info->wasign;
            else if ('(' == chr || ')' == chr)
            {
              //�ӱ��ʽ
              if ('(' == chr)
              {
                //�ӱ��ʽ��ʼ
                bool_info->type = FT_TOKEN_LEFT_PAREN;
              }
              else if (')' == chr)
              {
                //�ӱ��ʽ����
                bool_info->type = FT_TOKEN_RIGHT_PAREN;
                bool_info->yesno = bool_info->weight_adjust =
                  bool_info->wasign = bool_info->trunc = 0;
              }
              
              //�����ӱ��ʽ
              bool_info->position = 0;
              ret = param->mysql_add_word(param, NULL, 0, bool_info);
              if (0 != ret) break;
              
              //���ò���״̬
              bool_info->yesno = bool_info->weight_adjust =
                bool_info->wasign = bool_info->trunc = 0;
            }
            else if (' ' == chr)
            {
              //���ò���״̬
              bool_info->yesno = bool_info->weight_adjust =
                bool_info->wasign = bool_info->trunc = 0;
            }
            
            //�ַ�����ʼλ��
            start = next + nlen;
          }
        }
      }
    }
    else
    {
      //ת���
      //����ת��״̬
      escape = 0;
    }
    
    //�Ƿ�ĩβ
    next += nlen;
    if (end == next &&
      0 != (wlen = (int)(next - start)))
    {
      //�Ѵʴ��ݸ��ص�����
      bool_info->type = FT_TOKEN_WORD;
      ret = callback(doc, doc_len, start, wlen, param, bool_info);
      if (0 != ret) break;
    }
  }
  
  return(ret);
}

//����ģʽ����
static int atom_ngram_boolean_callback(
  char* doc, int doc_len, char* word, int len,
  MYSQL_FTPARSER_PARAM *param, MYSQL_FTPARSER_BOOLEAN_INFO *bool_info)
{
  int ret = 0, rlen;
  char *rword = atom_ngram_escape(word, len, &rlen, param);
  if (NULL != rword)
  {
    //ʹ�ü򵥽���
    ret = atom_ngram_parser(rword, rlen, atom_ngram_token_size, 
      atom_ngram_simple_callback, param, bool_info);
    free(rword);
  }
  else
  {
    //�����ڴ�ʧ��
    my_error(ER_OUTOFMEMORY, MYF(0), len);
    ret = 1;
  }
  
  return(ret);
}

/*
  Parse a document or a search query.

  SYNOPSIS
    atom_ngram_parser_parse()
      param              parsing context

  DESCRIPTION
    This is the main plugin function which is called to parse
    a document or a search query. The call mode is set in
    param->mode.  This function simply splits the text into words
    and passes every word to the MySQL full-text indexing engine.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int atom_ngram_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
  int ret = 0;
  MYSQL_FTPARSER_BOOLEAN_INFO bool_info =
    { FT_TOKEN_WORD, 0, 0, 0, 0, 0, ' ', 0 };

  //��ģʽ��ͣ��ģʽ
  //����������������ģʽ������
  if(MYSQL_FTPARSER_SIMPLE_MODE == param->mode ||
    MYSQL_FTPARSER_WITH_STOPWORDS == param->mode)
  {
    //ֱ�ӽ���
    ret = atom_ngram_parser(param->doc, param->length, 
      atom_ngram_token_size, atom_ngram_simple_callback, param, &bool_info);
  }
  
  //����ģʽ
  //֧��Ĭ���﷨�⣬���� \ ת���
  else if (MYSQL_FTPARSER_FULL_BOOLEAN_INFO == param->mode)
  {
    //�Ƚ�������ģʽ���﷨
    ret = atom_ngram_boolean_parser(param->doc, param->length,
      atom_ngram_boolean_callback, param, &bool_info);
  }
  
  return(ret);
}


/*
  Plugin type-specific descriptor
*/

static struct st_mysql_ftparser atom_ngram_parser_descriptor=
{
  MYSQL_FTPARSER_INTERFACE_VERSION, /* interface version      */
  atom_ngram_parser_parse,          /* parsing function       */
  atom_ngram_parser_init,           /* parser init function   */
  atom_ngram_parser_deinit          /* parser deinit function */
};

/*
  Plugin status variables for SHOW STATUS
*/

static struct st_mysql_show_var atom_ngram_status[]=
{
  {"atom_ngram_version", (char *)"v1.0", SHOW_CHAR, SHOW_SCOPE_GLOBAL},
  {0,0,0, SHOW_SCOPE_GLOBAL}
};

/*
  Plugin system variables.
*/

static MYSQL_SYSVAR_INT(token_size, atom_ngram_token_size,
  PLUGIN_VAR_READONLY, "atom ngram full text plugin parser token size in characters",
  NULL, NULL, 2, 1, 10, 0
);

static struct st_mysql_sys_var* atom_ngram_system_variables[]= {
  MYSQL_SYSVAR(token_size),
  NULL
};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(ftexample)
{
  MYSQL_FTPARSER_PLUGIN,          /* type                            */
  &atom_ngram_parser_descriptor,  /* descriptor                      */
  "atom_ngram",                   /* name                            */
  "hz86@live.cn",                 /* author                          */
  "Atom-Ngram Full-Text Parser",  /* description                     */
  PLUGIN_LICENSE_GPL,
  atom_ngram_parser_plugin_init,  /* init function (when loaded)     */
  atom_ngram_parser_plugin_deinit,/* deinit function (when unloaded) */
  0x0001,                         /* version                         */
  atom_ngram_status,              /* status variables                */
  atom_ngram_system_variables,    /* system variables                */
  NULL,
  0,
}
mysql_declare_plugin_end;

