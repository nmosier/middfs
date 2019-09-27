#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>

static int event_handler(struct module *module, int event, void *arg) {
  int err = 0;

  switch (event) {
  case MOD_LOAD:
    uprintf("Hello, kernel!\n");
    break;
  case MOD_UNLOAD:
    uprintf("Goodbye, kernel!\n");
    break;
  default:
    err = EOPNOTSUPP;
    break;
  }

  return err;
}

static moduledata_t hello_conf =
  {"hello",
   event_handler,
   NULL
  };


DECLARE_MODULE(hello, hello_conf, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);

