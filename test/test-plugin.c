/*
 * A minimal shared library for test-library.cpp to load at run time.
 *
 * Not part of the shipped library and not linked into anything: it is built
 * as its own .so purely so that the dynamic-loading tests have a real object
 * with known symbols to open, rather than reopening cutil itself, which is
 * already in the process and would let a broken loader appear to work.
 *
 * Built without -fvisibility=hidden, so these are exported under their plain
 * names with no version namespace -- which is also what a real plugin looks
 * like.
 */

int gcu_test_plugin_add(int a, int b) {
  return a + b;
}

const char * gcu_test_plugin_name(void) {
  return "ghoti-test-plugin";
}

int gcu_test_plugin_value = 1234;
