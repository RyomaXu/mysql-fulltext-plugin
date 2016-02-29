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


//atom-ngram 回调声明
typedef int(*ATOM_NGRAM_CALLBACK)(
  char* doc, int doc_len, char* word, int len, 
  MYSQL_FTPARSER_PARAM *param, MYSQL_FTPARSER_BOOLEAN_INFO *bool_info
);

//atom-ngram 解析器
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
    //计算字符长度，零或超出则跳出循环
    nlen = my_mbcharlen_ptr(param->cs, next, end);
    if(0 == nlen || next + nlen > end) {
      break;
	}
    
    //下一个，并记录词数
	if(0 == n) slen = nlen;
    next += nlen;
    n++;
    
    //词大小到达设置值或到字符串尾
    if (token_size == n || end == next)
    {
      //把词传递给回调函数
      ret = callback(doc, doc_len,
        start, (int)(next - start), param, bool_info);
      
      //返回错误或到达结尾则跳出
      if (0 != ret || end == next) {
        break;
      }
      
      //初始化
      start += slen;
      next = start;
      n = 0;
    }
  }
  
  return(ret);
}

//atom-ngram 简单模式处理
static int atom_ngram_simple_callback(
  char* doc, int doc_len, char* word, int len,
  MYSQL_FTPARSER_PARAM *param, MYSQL_FTPARSER_BOOLEAN_INFO *bool_info)
{
  bool_info->type = FT_TOKEN_WORD;
  bool_info->position = (int)(word - doc);
  return param->mysql_add_word(param, word, len, bool_info);
}

//字符串 \ 转义符处理
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

//atom-ngram boolean 回调声明
typedef int(*ATOM_NGRAM_BOOLEAN_CALLBACK)(
  char* doc, int doc_len, char* word, int len,
  MYSQL_FTPARSER_PARAM *param, MYSQL_FTPARSER_BOOLEAN_INFO *bool_info
);

//布尔模式解析
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
  
  //重置布尔状态
  bool_info->yesno = bool_info->weight_adjust =
    bool_info->wasign = bool_info->trunc = 0;
  
  //开始解析语法
  while (next < end)
  {
    //获取字符长度和类型
    nlen = param->cs->cset->ctype(
      param->cs, &ctype, (uchar *)next, (uchar *)end);
    
    //长度为零或超出则跳出循环
    if (0 == nlen || next + nlen > end) {
      break;
    }
    
    //是否转义
    if (0 == escape)
    {
      //ascii则继续解析
      if (0 != my_isascii(ctype))
      {
        chr = *next;
        if ('\\' == chr)
        {
          //检测到转义
          //转义状态开启
          escape = 1;
        }
        else if ('+' == chr || ' ' == chr || '-' == chr ||
          '>' == chr || '<' == chr || '~' == chr || '*' == chr ||
          '"' == chr || '(' == chr || ')' == chr)
        {
          //词截断处理
          //非引号状态 或 出现引号字符在处理
          if (('"' != chr && 0 == bool_info->quot) || '"' == chr)
          {
            //字符串长度非0
            if (0 != (wlen = (int)(next - start)))
            {
              //通配符和引号的处理
              if ('*' == chr) bool_info->trunc = 1;
              if (0 != bool_info->quot) bool_info->yesno = +1;
              
              //把词传递给回调函数
              bool_info->type = FT_TOKEN_WORD;
              ret = callback(doc, doc_len, start, wlen, param, bool_info);
              if (0 != ret) break;
              
              //重置布尔状态
              bool_info->yesno = bool_info->weight_adjust =
                bool_info->wasign = bool_info->trunc = 0;
            }
          }
          
          //优先处理引号
          if ('"' == chr || 0 != bool_info->quot)
          {
            //非引号则跳过
            if ('"' == chr)
            {
              //设置引号模式
              //开启子表达式和必须存在字符
              
              bool_info->yesno = +1;
              bool_info->position = 0;
              bool_info->quot = (0 == bool_info->quot ? (char *)1 : 0);
              bool_info->type = (0 != bool_info->quot ? FT_TOKEN_LEFT_PAREN : FT_TOKEN_RIGHT_PAREN);
              ret = param->mysql_add_word(param, NULL, 0, bool_info);
              if (0 != ret) break;
              
              //重置布尔状态
              bool_info->yesno = bool_info->weight_adjust =
                bool_info->wasign = bool_info->trunc = 0;
              
              //字符串开始位置
              start = next + nlen;
            }
          }
          else
          {
            //布尔模式语法解析
            if ('+' == chr) bool_info->yesno = +1;
            else if ('-' == chr) bool_info->yesno = -1;
            else if ('>' == chr) bool_info->weight_adjust++;
            else if ('<' == chr) bool_info->weight_adjust--;
            else if ('~' == chr) bool_info->wasign = !bool_info->wasign;
            else if ('(' == chr || ')' == chr)
            {
              //子表达式
              if ('(' == chr)
              {
                //子表达式开始
                bool_info->type = FT_TOKEN_LEFT_PAREN;
              }
              else if (')' == chr)
              {
                //子表达式结束
                bool_info->type = FT_TOKEN_RIGHT_PAREN;
                bool_info->yesno = bool_info->weight_adjust =
                  bool_info->wasign = bool_info->trunc = 0;
              }
              
              //设置子表达式
              bool_info->position = 0;
              ret = param->mysql_add_word(param, NULL, 0, bool_info);
              if (0 != ret) break;
              
              //重置布尔状态
              bool_info->yesno = bool_info->weight_adjust =
                bool_info->wasign = bool_info->trunc = 0;
            }
            else if (' ' == chr)
            {
              //重置布尔状态
              bool_info->yesno = bool_info->weight_adjust =
                bool_info->wasign = bool_info->trunc = 0;
            }
            
            //字符串开始位置
            start = next + nlen;
          }
        }
      }
    }
    else
    {
      //转义符
      //重置转义状态
      escape = 0;
    }
    
    //是否末尾
    next += nlen;
    if (end == next &&
      0 != (wlen = (int)(next - start)))
    {
      //把词传递给回调函数
      bool_info->type = FT_TOKEN_WORD;
      ret = callback(doc, doc_len, start, wlen, param, bool_info);
      if (0 != ret) break;
    }
  }
  
  return(ret);
}

//布尔模式处理
static int atom_ngram_boolean_callback(
  char* doc, int doc_len, char* word, int len,
  MYSQL_FTPARSER_PARAM *param, MYSQL_FTPARSER_BOOLEAN_INFO *bool_info)
{
  int ret = 0, rlen;
  char *rword = atom_ngram_escape(word, len, &rlen, param);
  if (NULL != rword)
  {
    //使用简单解析
    ret = atom_ngram_parser(rword, rlen, atom_ngram_token_size, 
      atom_ngram_simple_callback, param, bool_info);
    free(rword);
  }
  else
  {
    //分配内存失败
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

  //简单模式和停词模式
  //解析器忽略这两种模式的区别
  if(MYSQL_FTPARSER_SIMPLE_MODE == param->mode ||
    MYSQL_FTPARSER_WITH_STOPWORDS == param->mode)
  {
    //直接解析
    ret = atom_ngram_parser(param->doc, param->length, 
      atom_ngram_token_size, atom_ngram_simple_callback, param, &bool_info);
  }
  
  //布尔模式
  //支持默认语法外，增加 \ 转义符
  else if (MYSQL_FTPARSER_FULL_BOOLEAN_INFO == param->mode)
  {
    //先解析布尔模式的语法
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

