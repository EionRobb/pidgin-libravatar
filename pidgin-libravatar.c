#define PURPLE_PLUGINS

#include <glib.h>

#include <account.h>
#include <cipher.h>
#include <dnssrv.h>
#include <util.h>

#include <gtkutils.h>
#include <gtkplugin.h>

#define RAVATAR_BASE_URL "http://cdn.libravatar.org/avatar/"
#define PLUGIN_ID		"gtk-eionrobb-libravatar"
#define PREF_PREFIX		"/plugins/gtk/" PLUGIN_ID
#define PREF_EMAIL		PREF_PREFIX "/email"

static void got_ravatar(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	GList *cur;
	gchar *filename;

	g_return_if_fail(len != 0);
	
	filename = g_strconcat(g_get_tmp_dir(), G_DIR_SEPARATOR_S, purple_util_get_image_checksum(url_text, len), ".jpg", NULL);
	
	purple_util_write_data_to_file_absolute(filename, url_text, len);
	
	for(cur = purple_accounts_get_all();
		cur != NULL;
		cur = cur->next)
	{
		PurpleAccount *account = (PurpleAccount *) cur->data;
		PurplePlugin *plugin = purple_find_prpl(purple_account_get_protocol_id(account));
		
		size_t img_data_len;
		gpointer data;

		if (!plugin) continue;
		data = pidgin_convert_buddy_icon(plugin, filename, &img_data_len);
		
		if (!data) continue;
		purple_account_set_buddy_icon_path(account, filename);
		purple_buddy_icons_set_account_icon(account, data, img_data_len);
	}
	
	g_free(filename);
}

static const gchar *
ravatar_md5(const gchar *email)
{
	PurpleCipherContext *context;
	static gchar digest[41];
	gchar *lower_email;
	
	context = purple_cipher_context_new_by_name("md5", NULL);
	g_return_val_if_fail(context != NULL, NULL);
	
	lower_email = g_ascii_strdown(email, -1);
	g_strstrip(lower_email);
	purple_cipher_context_append(context, lower_email, strlen(lower_email));
	g_free(lower_email);
	
	if (!purple_cipher_context_digest_to_str(context, sizeof(digest), digest, NULL))
		return NULL;
	
	purple_cipher_context_destroy(context);
	
	return digest;
}

static void
ravatar_resolved_srv(PurpleSrvResponse *resp, int results, gpointer data)
{
	gchar *email = (gchar *) data;
	int i;
	gchar *ravatar_url = NULL;
	const gchar *md5_email;
	
	md5_email = ravatar_md5(email);
	if (results <= 0)
	{
		ravatar_url = g_strdup_printf(RAVATAR_BASE_URL "%s?d=404&s=256", md5_email);
	} else {
		//pick a random srv record to use
		//TODO don't use all the results, just the ones that have the same, equal, highest priority
		i = g_random_int_range(0, results);
		
		ravatar_url = g_strdup_printf("http://%s:%d/avatar/%s?s=256", resp[i].hostname, resp[i].port, md5_email);
	}
	
	if (ravatar_url)
	{
		purple_util_fetch_url(ravatar_url, TRUE, NULL, TRUE, got_ravatar, NULL);
	}
	
	g_free(ravatar_url);
	g_free(email);
}

static PurpleSrvTxtQueryData *
ravatar_resolve_url(const gchar *email)
{
	const gchar *domain;
	gchar **email_split;
	PurpleSrvTxtQueryData *resolve;
	
	email_split = g_strsplit(email, "@", 2);
	domain = email_split[1];
	
	//dig SRV _avatars._tcp.example.com
	resolve = purple_srv_resolve("avatars", "tcp", domain,ravatar_resolved_srv, g_strdup(email));
	
	g_strfreev(email_split);
	
	return resolve;
}

static gboolean
ravatar_resolve_timeout(gpointer data)
{
	ravatar_resolve_url(data);
	
	return FALSE;
}

static guint email_pref_changed_timeout = 0;

static void
ravatar_email_pref_changed(const gchar *name, PurplePrefType type, gconstpointer val, gpointer data)
{
	if (email_pref_changed_timeout)
		purple_timeout_remove(email_pref_changed_timeout);

	if (val && strlen(val))
	{
		email_pref_changed_timeout = purple_timeout_add_seconds(5, ravatar_resolve_timeout, (gpointer) val);
	}
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	const gchar *email;
	
	purple_prefs_connect_callback(plugin, PREF_EMAIL, ravatar_email_pref_changed, NULL);
	
	email = purple_prefs_get_string(PREF_EMAIL);
	if (email && *email)
		ravatar_resolve_url(email);
	
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	purple_prefs_disconnect_by_handle(plugin);
	return TRUE;
}

static PurplePluginPrefFrame *
plugin_config_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *pref;
	
	frame = purple_plugin_pref_frame_new();
	
    pref = purple_plugin_pref_new_with_label("Email address to use for account icon");
    purple_plugin_pref_frame_add(frame, pref);

    pref = purple_plugin_pref_new_with_name(PREF_EMAIL);
    purple_plugin_pref_frame_add(frame, pref);
	
	return frame;
}

static PurplePluginUiInfo prefs_info = {
	plugin_config_frame,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info = 
{
	PURPLE_PLUGIN_MAGIC,
	2,
	5,
	PURPLE_PLUGIN_STANDARD,
	PIDGIN_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,

	PLUGIN_ID,
	"libravatar Icons",
	"0.1",
	"Downloads libravatar icon as account icon",
	"libravatar is a service similar to Gravatar that lets you run your own libravatar server so that you can be in control of your avatar",
	"Eion Robb <eionrobb@gmail.com>",
	"", //URL
	
	plugin_load,
	plugin_unload,
	NULL,
	
	NULL,
	NULL,
	&prefs_info,
	NULL,
	
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
    purple_prefs_add_none(PREF_PREFIX);
    purple_prefs_add_string(PREF_EMAIL, "");
}

PURPLE_INIT_PLUGIN(libravatar, init_plugin, info);
