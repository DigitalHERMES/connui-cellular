#include <libosso.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>
#include <connui/connui-utils.h>
#include <connui/connui-log.h>
#include <icd/osso-ic-gconf.h>

#include <stdlib.h>
#include <time.h>

#include "context.h"

struct _connui_cell_datacounter
{
  gboolean initialized;
  GSList *notifiers;
  GConfClient *gconf;
  guint gconf_cnid;
  guint64 rx_bytes;
  guint64 tx_bytes;
  time_t reset_time;
  gboolean home;
  gchar *warning_limit;
  gboolean notification_enabled;
};

typedef struct _connui_cell_datacounter connui_cell_datacounter;

static connui_cell_datacounter datacounter;

static void
connui_cell_datacounter_notify(const connui_cell_datacounter *data)
{
  if (data && data->initialized)
  {
    connui_utils_notify_notify_UINT64_UINT64_TIMET_BOOLEAN_POINTER(
          data->notifiers,
          data->rx_bytes,
          data->tx_bytes,
          data->reset_time,
          data->notification_enabled,
          data->warning_limit);
  }
}

static guint64
connui_cell_datacounter_read_gconf_setting(
    const connui_cell_datacounter *counter, const gchar *name)
{
  gchar *s;
  guint64 rv;
  GError *error = NULL;

  s = gconf_client_get_string(counter->gconf, name, &error);

  if ( error )
  {
    CONNUI_ERR("%s: %s", name, error->message);
    g_clear_error(&error);
  }

  if (s)
    rv = strtoull(s, NULL, 10);
  else
    rv = 0LL;

  g_free(s);

  return rv;
}

static void
connui_cell_datacounter_gconf_changed(GConfClient *client, guint cnxn_id,
                                      GConfEntry *entry, gpointer user_data)
{
  connui_cell_datacounter *dc = user_data;
  GConfValue *val;
  const char *key;
  unsigned long long int counter;

  g_return_if_fail(dc != NULL && dc->initialized);

  val = gconf_entry_get_value(entry);
  key = gconf_entry_get_key(entry);

  if (val && val->type == GCONF_VALUE_STRING && gconf_value_get_string(val))
    counter = strtoull(gconf_value_get_string(val), NULL, 10);
  else
    counter = 0LL;

  if (dc->home)
  {
    if (!g_strcmp0(key, GPRS_HOME_RX_BYTES))
      dc->rx_bytes = counter;
    else if (!g_strcmp0(key, GPRS_HOME_TX_BYTES))
      dc->tx_bytes = counter;
    else if (!g_strcmp0(key, GPRS_HOME_RST_TIME))
      dc->reset_time = counter;
    else if (!g_strcmp0(key, GPRS_HOME_WARNING_LIMIT))
    {
      if (dc->warning_limit)
        g_free(dc->warning_limit);

      dc->warning_limit = g_strdup_printf("%llu", counter);
    }
    else if (!g_strcmp0(key, GPRS_HOME_NTFY_ENABLE))
    {
      dc->notification_enabled = gconf_value_get_bool(val);
    }
  }
  else
  {
    if (!g_strcmp0(key, GPRS_ROAM_RX_BYTES))
      dc->rx_bytes = counter;
    else if (g_strcmp0(key, GPRS_ROAM_TX_BYTES))
      dc->tx_bytes = counter;
    else if (!g_strcmp0(key, GPRS_ROAM_RST_TIME))
      dc->reset_time = counter;
    else if (!g_strcmp0(key, GPRS_ROAM_WARNING_LIMIT))
    {
      if (dc->warning_limit)
        g_free(dc->warning_limit);

      dc->warning_limit = g_strdup_printf("%llu", counter);
    }
    else if (!g_strcmp0(key, GPRS_ROAM_NTFY_ENABLE))
    {
      dc->notification_enabled = gconf_value_get_bool(val);
    }
  }

  connui_cell_datacounter_notify(dc);
  return;
}

static connui_cell_datacounter *
connui_cell_datacounter_get(gboolean home)
{
  gchar *s;
  GError *error = NULL;

  if (datacounter.initialized)
    return &datacounter;

  datacounter.gconf = NULL;
  datacounter.notifiers = NULL;
  datacounter.gconf_cnid = (guint)datacounter.initialized;
  datacounter.rx_bytes = 0;
  datacounter.tx_bytes = 0;
  datacounter.reset_time = 0;
  datacounter.home = home;
  datacounter.warning_limit = NULL;
  datacounter.notification_enabled = FALSE;
  datacounter.gconf = gconf_client_get_default();

  if (!datacounter.gconf)
  {
    CONNUI_ERR("Unable to get default GConf client");
    return NULL;
  }

  datacounter.gconf_cnid = gconf_client_notify_add(
        datacounter.gconf, ICD_GCONF_NETWORK_MAPPING_GPRS,
        connui_cell_datacounter_gconf_changed, &datacounter, NULL, &error);

  if (error)
  {
    CONNUI_ERR("Unable to add GConf notify: %s", error->message);
    g_clear_error(&error);
  }

  gconf_client_add_dir(datacounter.gconf, ICD_GCONF_NETWORK_MAPPING_GPRS,
                       GCONF_CLIENT_PRELOAD_ONELEVEL, &error);
  if (error)
  {
    CONNUI_ERR("Unable to add GConf notify dir: %s", error->message);
    g_clear_error(&error);
  }

  if (datacounter.home)
  {
    datacounter.rx_bytes =
        connui_cell_datacounter_read_gconf_setting(&datacounter, GPRS_HOME_RX_BYTES);
    datacounter.tx_bytes =
        connui_cell_datacounter_read_gconf_setting(&datacounter, GPRS_HOME_TX_BYTES);
    datacounter.reset_time =
        connui_cell_datacounter_read_gconf_setting(&datacounter, GPRS_HOME_RST_TIME);
    s = gconf_client_get_string(datacounter.gconf, GPRS_HOME_WARNING_LIMIT, &error);
    datacounter.warning_limit = g_strdup(s);

    if (error)
    {
      CONNUI_ERR(GPRS_HOME_WARNING_LIMIT ": %s", error->message);
      g_clear_error(&error);
    }

    datacounter.notification_enabled =
        gconf_client_get_bool(datacounter.gconf, GPRS_HOME_NTFY_ENABLE, &error);

    if (error)
    {
      CONNUI_ERR(GPRS_HOME_NTFY_ENABLE ": %s", error->message);
      g_clear_error(&error);
    }
  }
  else
  {
    datacounter.rx_bytes =
        connui_cell_datacounter_read_gconf_setting(&datacounter, GPRS_ROAM_RX_BYTES);
    datacounter.tx_bytes =
        connui_cell_datacounter_read_gconf_setting(&datacounter, GPRS_ROAM_TX_BYTES);
    datacounter.reset_time =
        connui_cell_datacounter_read_gconf_setting(&datacounter, GPRS_ROAM_RST_TIME);
    s = gconf_client_get_string(datacounter.gconf, GPRS_ROAM_WARNING_LIMIT, &error);
    datacounter.warning_limit = g_strdup(s);

    if (error)
    {
      CONNUI_ERR(GPRS_ROAM_WARNING_LIMIT ": %s", error->message);
      g_clear_error(&error);
    }

    datacounter.notification_enabled =
        gconf_client_get_bool(datacounter.gconf, GPRS_ROAM_NTFY_ENABLE, &error);

    if (error)
    {
      CONNUI_ERR(GPRS_ROAM_NTFY_ENABLE ": %s", error->message);
      g_clear_error(&error);
    }
  }

  datacounter.initialized = TRUE;

  return &datacounter;
}

void
connui_cell_datacounter_close(cell_datacounter_cb cb)
{
  connui_cell_datacounter *data = connui_cell_datacounter_get(TRUE);

  g_return_if_fail(data != NULL && data->initialized);

  if (cb)
    data->notifiers = connui_utils_notify_remove(data->notifiers, cb);

  if (!data->notifiers)
  {
    gconf_client_remove_dir(data->gconf, ICD_GCONF_NETWORK_MAPPING_GPRS, NULL);
    gconf_client_notify_remove(data->gconf, data->gconf_cnid);
    g_object_unref(data->gconf);
    data->gconf = NULL;
    g_free(data->warning_limit);
    data->initialized = FALSE;
    data->warning_limit = NULL;
  }
}

static void
connui_cell_datacounter_write_gconf_setting(connui_cell_datacounter *dc,
                                            const gchar *dc_name,
                                            unsigned long long int val)
{
  gchar *s = g_strdup_printf("%llu", val);
  GError *error = NULL;

  gconf_client_set_string(dc->gconf, dc_name, s, &error);

  if (error)
  {
    CONNUI_ERR("%s: %s", dc_name, error->message);
    g_clear_error(&error);
  }

  g_free(s);
}

void
connui_cell_datacounter_reset()
{
  connui_cell_datacounter *dc = connui_cell_datacounter_get(TRUE);
  GError *error = NULL;

  if (dc->home)
  {
    connui_cell_datacounter_write_gconf_setting(dc, GPRS_HOME_RST_TIME, time(NULL));
    gconf_client_unset(dc->gconf, GPRS_HOME_RX_BYTES, &error);

    if (error)
    {
      CONNUI_ERR("Unable to unset home RX bytes: %s", error->message);
      g_clear_error(&error);
    }

    gconf_client_unset(dc->gconf, GPRS_HOME_TX_BYTES, &error);

    if (error)
    {
      CONNUI_ERR("Unable to unset home TX bytes: %s", error->message);
      g_clear_error(&error);
    }

    gconf_client_unset(dc->gconf, GPRS_HOME_LAST_NTFY, &error);

    if (error)
    {
      CONNUI_ERR("Unable to unset last home notification: %s",
                 error->message);
      g_clear_error(&error);
    }
  }
  else
  {
    connui_cell_datacounter_write_gconf_setting(dc, GPRS_ROAM_RST_TIME, time(NULL));
    gconf_client_unset(dc->gconf, GPRS_ROAM_RX_BYTES, &error);

    if (error)
    {
      CONNUI_ERR("Unable to unset roaming RX bytes: %s", error->message);
      g_clear_error(&error);
    }

    gconf_client_unset(dc->gconf, GPRS_ROAM_TX_BYTES, &error);

    if (error)
    {
      CONNUI_ERR("Unable to unset roaming TX bytes: %s", error->message);
      g_clear_error(&error);
    }

    gconf_client_unset(dc->gconf, GPRS_ROAM_LAST_NTFY, &error);

    if (error)
    {
      CONNUI_ERR("Unable to unset last roaming notification: %s",
                 error->message);
      g_clear_error(&error);
    }
  }

  connui_cell_datacounter_close(NULL);
}

gboolean
connui_cell_datacounter_register(cell_datacounter_cb cb, gboolean home,
                                 gpointer user_data)
{
  connui_cell_datacounter *dc = connui_cell_datacounter_get(home);

  g_return_val_if_fail(dc != NULL && dc->initialized, FALSE);

  dc->notifiers = connui_utils_notify_add(dc->notifiers, cb, user_data);
  connui_cell_datacounter_notify(dc);

  return TRUE;
}

void
connui_cell_datacounter_save(gboolean notification_enabled,
                             const gchar *warning_limit)
{
  connui_cell_datacounter *dc = connui_cell_datacounter_get(TRUE);
  GError *error = NULL;

  if (dc->home)
  {
    gconf_client_set_bool(dc->gconf, GPRS_HOME_NTFY_ENABLE, notification_enabled,
                          &error);

    if (error)
    {
      CONNUI_ERR(GPRS_HOME_NTFY_ENABLE ": %s", error->message);
      g_clear_error(&error);
    }

    if (warning_limit)
    {
      gconf_client_set_string(dc->gconf, GPRS_HOME_WARNING_LIMIT, warning_limit,
                              &error);

      if (error)
      {
        CONNUI_ERR(GPRS_HOME_WARNING_LIMIT ": %s", error->message);
        g_clear_error(&error);
      }

      if (notification_enabled)
      {
        /* WTF ? */
        gchar *s = g_strconcat(warning_limit, "000000", NULL);

        gconf_client_set_string(dc->gconf, GPRS_HOME_NTFY_PERIOD, s, &error);

        if (error)
        {
          CONNUI_ERR(GPRS_HOME_NTFY_PERIOD ": %s", error->message);
          g_clear_error(&error);
        }

        g_free(s);
      }
      else
      {
        gconf_client_set_string(dc->gconf, GPRS_HOME_NTFY_PERIOD, "0", &error);

        if (error)
        {
          CONNUI_ERR(GPRS_HOME_NTFY_PERIOD ": %s", error->message);
          g_clear_error(&error);
        }
      }
    }
  }
  else
  {
    gconf_client_set_bool(dc->gconf, GPRS_ROAM_NTFY_ENABLE, notification_enabled,
                          &error);
    if (error)
    {
      CONNUI_ERR(GPRS_ROAM_NTFY_ENABLE ": %s", error->message);
      g_clear_error(&error);
    }

    if (warning_limit)
    {
      gconf_client_set_string(dc->gconf, GPRS_ROAM_WARNING_LIMIT, warning_limit,
                              &error);
      if (error)
      {
        CONNUI_ERR(GPRS_ROAM_WARNING_LIMIT ": %s", error->message);
        g_clear_error(&error);
      }

      if (notification_enabled)
      {
        gchar *s = g_strconcat(warning_limit, "000000", NULL);

        gconf_client_set_string(dc->gconf, GPRS_ROAM_NTFY_PERIOD, s, &error);

        if (error)
        {
          CONNUI_ERR(GPRS_HOME_NTFY_PERIOD ": %s", error->message);
          g_clear_error(&error);
        }

        g_free(s);
      }
      else
      {
        gconf_client_set_string(dc->gconf, GPRS_ROAM_NTFY_PERIOD, "0", &error);

        if (error)
        {
          CONNUI_ERR(GPRS_HOME_NTFY_PERIOD ": %s", error->message);
          g_clear_error(&error);
        }
      }
    }
  }
}
