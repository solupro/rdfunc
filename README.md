rdfunc
======

A PHP Extension support wildcard-character(*) to disabled functions

####Install

```shell
$/path/to/phpize
$./configure --with-php-config=/path/to/php-config/
$make && make install
```

####Example

- edit your `php.ini`

```ini
[rdfunc]
extension=rdfunc.so
rdfunc.disable_functions=array_*,str_*
```

- restart server

test.php

```php
<?php
$arr = array(1, 2, 3);
array_pop($arr);
$str = 'foobar';
$str = str_replace('foo', 'baz', $str);

var_dump($arr, $str);
```

- view the page

ouput

```
Warning: rdfunc: array_pop has been disabled! (°Д°≡°д°)ｴｯ!? in /Users/solu/Workspace/PHP/index.php on line 3

Warning: rdfunc: str_replace has been disabled! (°Д°≡°д°)ｴｯ!? in /Users/solu/Workspace/PHP/index.php on line 5
array(3) { [0]=> int(1) [1]=> int(2) [2]=> int(3) } NULL
```

#### Blog (in Chinese)

- <http://my.oschina.net/s01u/blog/107911>

