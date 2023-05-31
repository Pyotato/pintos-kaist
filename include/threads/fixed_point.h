/*

* 소수점 연산 구현
 * priority, nice, ready_threads값은 정수, recent_cpu, load_avg값은 실수이다.
 *  핀토스는 커널에서 부동소수점 연산을 지원하지 않는다
 * recent_cpu(최근에 얼마나 많은 CPU time을 사용했는가)와 load_avg(최근 1분 동안 수행 가능한 프로세스의 평균 개수)값을 계산을 위하여 소수점 연산이 필요하다.
 * 17.14 fixed-point number representation을 이용하여 소수점 연산을 구현

제일 왼쪽 한비트    17 비트를 정수         오른쪽 14비트를 소숫점
------------------------------------------------------------------
|           |                           |                       |
| 부호 비트 |       정수 부분(17bits)   |   소수점부분(14bits)  |
|           |                           |                       |
-----------------------------------------------------------------


*/

#ifndef ARITHMETIC_H
#define ARITHMETIC_H

int integer_to_float(int n);
int float_to_integer(int x);
int add_x_n(int x, int n);
int sub_n_x(int x, int n);
int mul_x_y(int x, int y);
int div_x_y(int x, int y);

#endif /* threads/fixed_point.h */