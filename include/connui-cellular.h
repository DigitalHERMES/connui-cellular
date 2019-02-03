#ifndef __CONNUI_CELLULAR_H__
#define __CONNUI_CELLULAR_H__

struct _cell_network
{
  guchar service_status;
  gchar *country_code;
  gchar *operator_code;
  gchar *operator_name;
  guchar umts_avail;
  guchar network_type;
};
typedef struct _cell_network cell_network;

struct _cell_network_state
{
  gint reg_status;
  guint lac;
  guint cell_id;
  cell_network *network;
  guchar supported_services;
  guchar network_signals_bar;
  guchar rat_name;
  guchar network_hsdpa_allocated;
  guchar operator_name_type;
  gchar *operator_name;
  gchar *alternative_operator_name;
};

typedef struct _cell_network_state cell_network_state;

typedef void (*cell_call_status_cb) (gboolean calls, gpointer user_data);
typedef void (*cell_cs_status_cb) (gboolean active, gpointer user_data);
typedef void (*cell_network_state_cb) (const cell_network_state *state,
                                       gpointer user_data);
typedef void (*cell_sim_status_cb) (guint status, gpointer user_data);
typedef void (*cell_ssc_state_cb) (const gchar *state, gpointer user_data);

void connui_cell_network_free(cell_network *network);
cell_network *connui_cell_network_dup(const cell_network *network);

enum net_registration_status
{
    NETWORK_REG_STATUS_HOME = 0x00, // CS is registered to home network
    NETWORK_REG_STATUS_ROAM,        // CS is registered to some other network than home network
    NETWORK_REG_STATUS_ROAM_BLINK,  // CS is registered to non-home system in a non-home area ('a' or 'b' area)
    NETWORK_REG_STATUS_NOSERV,      // CS is not in service
    NETWORK_REG_STATUS_NOSERV_SEARCHING,    // CS is not in service, but is currently searching for service
    NETWORK_REG_STATUS_NOSERV_NOTSEARCHING, // CS is not in service and it is not currently searching for service
    NETWORK_REG_STATUS_NOSERV_NOSIM,        // CS is not in service due to missing SIM or missing subscription
    NETWORK_REG_STATUS_POWER_OFF = 0x08,    // CS is in power off state
    NETWORK_REG_STATUS_NSPS,                // CS is in No Service Power Save State (currently not listening to any cell)
    NETWORK_REG_STATUS_NSPS_NO_COVERAGE,    // CS is in No Service Power Save State (CS is entered to this state because there is no network coverage)
    NETWORK_REG_STATUS_NOSERV_SIM_REJECTED_BY_NW // CS is not in service due to missing subscription
};

enum network_alpha_tag_name_type
{
// The operator name contains only 'ordinary' characters. The operator name is
// from the hard-coded operator name list
    NETWORK_HARDCODED_LATIN_OPER_NAME = 0,
// The operator name contains some language specific characters (like Chinese).
// The operator name is from the hard-coded operator name list
    NETWORK_HARDCODED_USC2_OPER_NAME,
// The operator name is received from the cellular network in a special message
// containing the operator name. The network has indicated that this name is a
// short name.
    NETWORK_NITZ_SHORT_OPER_NAME,
// The operator name is received from the cellular network in a special message
// containing the operator name. The network has indicated that this is a full
// name operator name.
    NETWORK_NITZ_FULL_OPER_NAME,
// The operator name from SIM card
    NETWORK_SIM_OPER_NAME = 0x08,
// The operator name is received from the SIM and it is mapped to certain LAC.
// The SIM has indicated that this name is a short name.
    NETWORK_EONS_SHORT_OPER_NAME,
// The operator name is received from the SIM and it is mapped to certain LAC.
// The SIM has indicated that this is a full name
    NETWORK_EONS_FULL_OPER_NAME
};

#define NETWORK_RAT_NAME_UNKNOWN         0x00
#define NETWORK_GSM_RAT                  0x01
#define NETWORK_UMTS_RAT                 0x02

#define NETWORK_MASK_GPRS_SUPPORT   0x01
#define NETWORK_MASK_CS_SERVICES    0x02
#define NETWORK_MASK_EGPRS_SUPPORT  0x04
#define NETWORK_MASK_HSDPA_AVAIL    0x08
#define NETWORK_MASK_HSUPA_AVAIL    0x10

// Network selection mode is unset or does not matter
#define NETWORK_SELECT_MODE_UNKNOWN       0x00

// Network selection mode is manual
#define NETWORK_SELECT_MODE_MANUAL        0x01

// Network selection mode is automatic
#define NETWORK_SELECT_MODE_AUTOMATIC     0x02

// CS performs user reselection. This is currently GSM-specific procedure, which is
// specified in GSM TS 03.22.
#define NETWORK_SELECT_MODE_NO_SELECTION  0x04

// Network status availability of operators
#define NETWORK_OPER_STATUS_UNKNOWN       0x00  // It is not known in which category operator belongs
#define NETWORK_OPER_STATUS_AVAILABLE     0x01  // The operator is heard and allowed
#define NETWORK_OPER_STATUS_CURRENT       0x02  // The CS is currently in service in this operator
#define NETWORK_OPER_STATUS_FORBIDDEN     0x03  // The operator is forbidden

// Selected rat type
#define NET_GSS_DUAL_SELECTED_RAT            0x00
#define NET_GSS_GSM_SELECTED_RAT             0x01
#define NET_GSS_UMTS_SELECTED_RAT            0x02
#define NET_GSS_UNKNOWN_SELECTED_RAT         0x03

/* NET */
gboolean connui_cell_net_status_register(cell_network_state_cb cb, gpointer user_data);
void connui_cell_net_status_close(cell_network_state_cb cb);
gchar *connui_cell_net_get_operator_name(cell_network *network, gboolean long_name, gint *error_value);
guchar connui_cell_net_get_network_selection_mode(gint *error_value);
guchar connui_cell_net_get_radio_access_mode(gint *error_value);
void connui_cell_net_cancel_service_call(guint call_id);
void connui_cell_cs_status_close(cell_cs_status_cb cb);
gboolean connui_cell_net_is_activated(gint *error_value);

/* SIM */
gboolean connui_cell_sim_status_register(cell_sim_status_cb cb, gpointer user_data);
void connui_cell_sim_status_close(cell_sim_status_cb cb);
gboolean connui_cell_sim_is_network_in_service_provider_info(gint *error_value, guchar *code);
gchar *connui_cell_sim_get_service_provider(guint *name_type, gint *error_value);

/* SSC */
gboolean connui_cell_ssc_state_register(cell_ssc_state_cb cb, gpointer user_data);
void connui_cell_ssc_state_close(cell_ssc_state_cb cb);

/* CALL */
void connui_cell_call_status_close(cell_call_status_cb cb);

/* CODE UI */
gboolean connui_cell_code_ui_cancel_dialog();
gboolean connui_cell_code_ui_is_sim_locked_with_error();
gboolean connui_cell_sim_is_locked(gboolean *has_error);

#endif /* __CONNUI_CELLULAR_H__ */
