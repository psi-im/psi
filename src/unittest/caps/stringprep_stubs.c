// -----------------------------------------------------------------------------
// Stringprep stubs
// -----------------------------------------------------------------------------

#include <stringprep.h>

Stringprep_profile stringprep_nameprep[1];
Stringprep_profile stringprep_xmpp_nodeprep[1];
Stringprep_profile stringprep_xmpp_resourceprep[1];

int stringprep(char* n, size_t maxlen, Stringprep_profile_flags flags, Stringprep_profile* profile)
{
	return 0;
}
