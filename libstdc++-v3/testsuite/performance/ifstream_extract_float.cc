// Copyright (C) 2004 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

#include <fstream>
#include <testsuite_performance.h>

int main() 
{
  using namespace std;
  using namespace __gnu_test;

  time_counter time;
  resource_counter resource;
  const int iterations = 10000000;

  {
    ofstream out("tmp_perf_float.txt");
    for (int i = 0; i < iterations; ++i)
      {
	float f = i * 3.14159265358979323846;
	out << f << "\n";
      }
  }

  {
    ifstream in("tmp_perf_float.txt");
    float f;
    start_counters(time, resource);  
    for (int j, i = 0; i < iterations; ++i)
      in >> f;
    stop_counters(time, resource);
    report_performance(__FILE__, "", time, resource);
  }

  unlink("tmp_perf_int.txt");
  return 0;
};
