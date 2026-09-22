/*
 * A shared library with an unresolvable symbol, for one test.
 *
 * Built with no --no-undefined, so it links: the reference below is left for
 * the dynamic loader to resolve, and there is nothing to resolve it with.
 * RTLD_NOW must therefore refuse to open it, and RTLD_LAZY would happily
 * succeed and crash later at the first call. That difference is the only
 * thing this file exists to make visible.
 */

extern int gcu_test_plugin_no_such_function(void);

int gcu_test_broken_entry(void) {
  return gcu_test_plugin_no_such_function();
}
