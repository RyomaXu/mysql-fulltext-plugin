# mysql-fulltext-plugin
MYSQL 全文索引插件，基于ngram算法，布尔搜索增加转义字符语法，支付搜索任意字符。 

=================================================================================

使用说明：

把DLL放到 "mysql\lib\plugin"

安装插件
win:   INSTALL PLUGIN atom_ngram SONAME 'atom_ngram_parser.dll';
linux: INSTALL PLUGIN atom_ngram SONAME 'atom_ngram_parser.so';

卸载插件
UNINSTALL PLUGIN atom_ngram;

查看插件是否开启
SHOW PLUGINS;

给数据库添加索引
ALTER TABLE `数据库名.表名` ADD FULLTEXT INDEX `全文索引名` (`字段名`) WITH PARSER atom_ngram;
ALTER TABLE `数据库名.表名` ADD FULLTEXT INDEX `全文联合索引名` (`字段名1`,`字段名2`) WITH PARSER atom_ngram;

重建索引
REPAIR TABLE `数据库名.表名` QUICK;

=================================================================================

my.ini

InnoDB 设置关键词长度 
innodb_ft_min_token_size
innodb_ft_max_token_size

MyISAM 设置关键词长度 
ft_min_word_len
ft_max_word_len

atom_ngram插件 取值范围 1-10 默认2
atom_ngram_token_size

==================================================================================

查询语句（MATCH字段必须和索引一致）
关键词支持转义符 \

SELECT * FROM `数据库名.表名` WHERE MATCH(`字段名`) AGAINST ('关键词');
SELECT * FROM `数据库名.表名` WHERE MATCH(`字段名`) AGAINST ('关键词' IN BOOLEAN MODE);

SELECT * FROM `数据库名.表名` WHERE MATCH(`字段名1`,`字段名2`) AGAINST ('关键词');
SELECT * FROM `数据库名.表名` WHERE MATCH(`字段名1`,`字段名2`) AGAINST ('关键词' IN BOOLEAN MODE);


使用：IN BOOLEAN MODE
支持以下语法：

1.      'apple banana'
寻找包含至少两个单词中的一个的行。

2.      '+apple +juice'
寻找两个单词都包含的行。
 
3.      '+apple macintosh'
寻找包含单词“apple”的行，若这些行也包含单词“macintosh”，则列为更高等级。

4.      '+apple -macintosh'
寻找包含单词“apple”但不包含单词 “macintosh”的行。

5.      '+apple ~macintosh'
查找包含单词“apple”的行，但如果该行也包含单词“macintosh”，则列为低等级。

6.      '+apple +(>turnover <strudel)'
寻找包含单词“apple”和“turnover”的行，或包含“apple”和“strudel”的行 (无先后顺序),然而包含“apple turnover”的行较包含“apple strudel”的行排列等级更为高。

7.      'apple*'
寻找包含“apple”、“apples”、“applesauce”或“applet”的行。

8.      '"some words"'
寻找包含原短语“some words”的行 (例如,包含“some words of wisdom”的行，而非包含 “some noisewords”的行)。注意包围词组的‘"’符号是界定短语的操作符字符。它们不是包围搜索字符串本身的引号。

9.      'c\\+\\+'
使用转义符，原型 c++ 
