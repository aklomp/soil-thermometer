#include <osapi.h>
#include <user_interface.h>

#include "missing.h"
#include "state.h"

// Post a state message to the user task
void ICACHE_FLASH_ATTR
state_change (enum state state)
{
	if (!system_os_post(USER_TASK_PRIO_0, state, 0))
		os_printf("state_change() failed!\n");
}
