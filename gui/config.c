#include "config.h"

#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui/ui_util.h"

#include "platform.h"

vpi_config_t vpi_config;

void hex_to_string(char *output, uint8_t *data, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        sprintf(output + i * 2, "%02x", data[i] & 0xFF);
    }
}

void string_to_hex(unsigned char *output, size_t length, const char *input)
{
    char c[3];
    c[2] = 0;
    for (size_t i = 0; i < length; i++) {
        memcpy(c, input + i * 2, 2);
        output[i] = strtol(c, 0, 16);
    }
}

static void vpi_config_reset_default_controls_internal()
{
    for (int i = 0; i < VPI_CONFIG_BUTTONMAP_SIZE; i++) vpi_config.buttonmap[i] = VPI_CONFIG_UNMAPPED;
    for (int i = 0; i < VPI_CONFIG_AXISMAP_SIZE; i++) vpi_config.axismap[i] = VPI_CONFIG_UNMAPPED;
    for (int i = 0; i < VPI_CONFIG_KEYMAP_SIZE; i++) vpi_config.keymap[i] = VPI_CONFIG_UNMAPPED;
}

void vpi_config_save()
{
    char config_fn[1024];
    vpi_config_filename(config_fn, sizeof(config_fn));
    xmlTextWriterPtr writer = xmlNewTextWriterFilename(config_fn, 0);
    if (!writer) {
        vpilog("Error: xmlNewTextWriterFilename\n");
        return;
    }

    char buf[0x100];

    xmlTextWriterStartDocument(writer, 0, 0, 0);

    xmlTextWriterSetIndent(writer, 1);
    xmlTextWriterSetIndentString(writer, BAD_CAST "\t");

    xmlTextWriterStartElement(writer, BAD_CAST "vanilla");

    xmlTextWriterStartElement(writer, BAD_CAST "consoles");

    for (uint8_t i = 0; i < vpi_config.connected_console_count; i++) {
        vpi_console_entry_t *entry = &vpi_config.connected_console_entries[i];
        xmlTextWriterStartElement(writer, BAD_CAST "console");
        xmlTextWriterWriteElement(writer, BAD_CAST "name", BAD_CAST entry->name);
        hex_to_string(buf, entry->bssid.bssid, sizeof(vanilla_bssid_t));
        xmlTextWriterWriteElement(writer, BAD_CAST "bssid", BAD_CAST buf);
        hex_to_string(buf, entry->psk.psk, sizeof(vanilla_psk_t));
        xmlTextWriterWriteElement(writer, BAD_CAST "psk", BAD_CAST buf);
        xmlTextWriterEndElement(writer); // console
    }
    xmlTextWriterEndElement(writer); // consoles

    sprintf(buf, "%X", vpi_config.server_address);
    xmlTextWriterWriteElement(writer, BAD_CAST "server", BAD_CAST buf);

    xmlTextWriterWriteElement(writer, BAD_CAST "wireless", BAD_CAST vpi_config.wireless_interface);

    sprintf(buf, "%i", vpi_config.connection_setup);
    xmlTextWriterWriteElement(writer, BAD_CAST "connectionsetup", BAD_CAST buf);

    sprintf(buf, "%i", vpi_config.region);
    xmlTextWriterWriteElement(writer, BAD_CAST "region", BAD_CAST buf);

    sprintf(buf, "%i", vpi_config.swap_abxy);
    xmlTextWriterWriteElement(writer, BAD_CAST "swapabxy", BAD_CAST buf);

    sprintf(buf, "%i", vpi_config.fullscreen);
    xmlTextWriterWriteElement(writer, BAD_CAST "fullscreen", BAD_CAST buf);

    xmlTextWriterStartElement(writer, BAD_CAST "controls");
    if (vpi_config.keymap) {
        xmlTextWriterStartElement(writer, BAD_CAST "keys");

        for (int i = 0; i < VPI_CONFIG_KEYMAP_SIZE; i++) {
            int val = vpi_config.keymap[i];
            if (val != VPI_CONFIG_UNMAPPED) {
                xmlTextWriterStartElement(writer, BAD_CAST "key");

                sprintf(buf, "%i", i);
                xmlTextWriterWriteAttribute(writer, BAD_CAST "id", BAD_CAST buf);

                sprintf(buf, "%i", val);
                xmlTextWriterWriteString(writer, BAD_CAST buf);

                xmlTextWriterEndElement(writer); // key
            }
        }

        xmlTextWriterEndElement(writer); // keys
    }

    if (vpi_config.buttonmap) {
        xmlTextWriterStartElement(writer, BAD_CAST "buttons");

        for (int i = 0; i < VPI_CONFIG_BUTTONMAP_SIZE; i++) {
            int val = vpi_config.buttonmap[i];
            if (val != VPI_CONFIG_UNMAPPED) {
                xmlTextWriterStartElement(writer, BAD_CAST "button");

                sprintf(buf, "%i", i);
                xmlTextWriterWriteAttribute(writer, BAD_CAST "id", BAD_CAST buf);

                sprintf(buf, "%i", val);
                xmlTextWriterWriteString(writer, BAD_CAST buf);

                xmlTextWriterEndElement(writer); // button
            }
        }

        xmlTextWriterEndElement(writer); // buttons
    }

    if (vpi_config.axismap) {
        xmlTextWriterStartElement(writer, BAD_CAST "axes");

        for (int i = 0; i < VPI_CONFIG_AXISMAP_SIZE; i++) {
            int val = vpi_config.axismap[i];
            if (val != VPI_CONFIG_UNMAPPED) {
                xmlTextWriterStartElement(writer, BAD_CAST "axis");

                sprintf(buf, "%i", i);
                xmlTextWriterWriteAttribute(writer, BAD_CAST "id", BAD_CAST buf);

                sprintf(buf, "%i", val);
                xmlTextWriterWriteString(writer, BAD_CAST buf);

                xmlTextWriterEndElement(writer); // axis
            }
        }

        xmlTextWriterEndElement(writer); // axes
    }
    xmlTextWriterEndElement(writer); // controls

    xmlTextWriterEndElement(writer); // vanilla

    xmlTextWriterEndDocument(writer);

    xmlFreeTextWriter(writer);
}

void vpi_config_init()
{
    LIBXML_TEST_VERSION;

    // Set defaults
    memset(&vpi_config, 0, sizeof(vpi_config));

    vpi_config.server_address = VANILLA_ADDRESS_LOCAL;
    vpi_config.region = VANILLA_REGION_AMERICA;
    vpi_config.fullscreen = 1;
    vpi_config_reset_default_controls_internal();

    // Load from file
    char config_fn[1024];
    vpi_config_filename(config_fn, sizeof(config_fn));

    xmlDocPtr doc = xmlReadFile(config_fn, 0, 0);
    if (doc) {
        xmlNodePtr root = xmlDocGetRootElement(doc);
        if (root) {
            xmlNodePtr child = root->children;
            while (child) {
                if (child->type == XML_ELEMENT_NODE) {
                    if (!strcmp((const char *) child->name, "consoles")) {
                        // Firstly, count the number of consoles
                        xmlNodePtr console = child->children;
                        while (console) {
                            if (console->type == XML_ELEMENT_NODE && !strcmp((const char *) console->name, "console")) {
                                vpi_config.connected_console_count++;
                            }
                            console = console->next;
                        }

                        // Allocate memory for console entries
                        vpi_config.connected_console_entries = malloc(vpi_config.connected_console_count * sizeof(vpi_console_entry_t));

                        vpi_console_entry_t *entry = vpi_config.connected_console_entries;
                        console = child->children;
                        while (console) {
                            if (console->type == XML_ELEMENT_NODE && !strcmp((const char *) console->name, "console")) {
                                xmlNodePtr console_info = console->children;
                                while (console_info) {
                                    if (!strcmp((const char *) console_info->name, "name")) {
                                        vui_strncpy(entry->name, (const char *) console_info->children->content, sizeof(entry->name));
                                    } else if (!strcmp((const char *) console_info->name, "bssid")) {
                                        string_to_hex(entry->bssid.bssid, sizeof(entry->bssid), (const char *) console_info->children->content);
                                    } else if (!strcmp((const char *) console_info->name, "psk")) {
                                        string_to_hex(entry->psk.psk, sizeof(entry->psk), (const char *) console_info->children->content);
                                    }
                                    console_info = console_info->next;
                                }
                                entry++;
                            }
                            console = console->next;
                        }
                    } else if (!strcmp((const char *) child->name, "server")) {
                        vpi_config.server_address = strtoul((const char *) child->children->content, 0, 16);
                    } else if (!strcmp((const char *) child->name, "wireless")) {
                        if (child->children) {
                            vui_strncpy(vpi_config.wireless_interface, (const char *) child->children->content, sizeof(vpi_config.wireless_interface));
                        }
                    } else if (!strcmp((const char *) child->name, "connectionsetup")) {
                        vpi_config.connection_setup = atoi((const char *) child->children->content);
                    } else if (!strcmp((const char *) child->name, "region")) {
                        vpi_config.region = atoi((const char *) child->children->content);
                    } else if (!strcmp((const char *) child->name, "swapabxy")) {
                        vpi_config.swap_abxy = atoi((const char *) child->children->content);
                    } else if (!strcmp((const char *) child->name, "fullscreen")) {
                        vpi_config.fullscreen = atoi((const char *) child->children->content);
                    } else if (!strcmp((const char *) child->name, "controls")) {
                        xmlNodePtr section = child->children;
                        while(section){
                            if (section->type == XML_ELEMENT_NODE) {
                                if (!strcmp(section->name, "keys")) {
                                    xmlNodePtr key = section->children;
                                    while (key) {
                                        if (key->type == XML_ELEMENT_NODE && !strcmp(key->name, "key")) {
                                            int id = -1;
                                            xmlAttr *attribute = key->properties;
                                            while (attribute) {
                                                if (!strcmp(attribute->name, "id")) {
                                                    id = atoi((const char *) attribute->children->content);
                                                }
                                                attribute = attribute->next;
                                            }

                                            if (id == -1) {
                                                continue;
                                            }

                                            int val = atoi((const char *) key->children->content);
                                            vpi_config.keymap[id] = val;
                                        }
                                        key = key->next;
                                    }
                                } else if (!strcmp(section->name, "buttons")) {
                                    xmlNodePtr btn = section->children;
                                    while (btn) {
                                        if (btn->type == XML_ELEMENT_NODE && !strcmp(btn->name, "button")) {
                                            int id = -1;
                                            xmlAttr *attribute = btn->properties;
                                            while (attribute) {
                                                if (!strcmp(attribute->name, "id")) {
                                                    id = atoi((const char *) attribute->children->content);
                                                }
                                                attribute = attribute->next;
                                            }

                                            if (id == -1) {
                                                continue;
                                            }

                                            int val = atoi((const char *) btn->children->content);
                                            vpi_config.buttonmap[id] = val;
                                        }
                                        btn = btn->next;
                                    }
                                } else if (!strcmp(section->name, "axes")) {
                                    xmlNodePtr axis = section->children;
                                    while (axis) {
                                        if (axis->type == XML_ELEMENT_NODE && !strcmp(axis->name, "axis")) {
                                            int id = -1;
                                            xmlAttr *attribute = axis->properties;
                                            while (attribute) {
                                                if (!strcmp(attribute->name, "id")) {
                                                    id = atoi((const char *) attribute->children->content);
                                                }
                                                attribute = attribute->next;
                                            }

                                            if (id == -1) {
                                                continue;
                                            }

                                            int val = atoi((const char *) axis->children->content);
                                            vpi_config.axismap[id] = val;
                                        }
                                        axis = axis->next;
                                    }
                                }
                            }
                            section = section->next;
                        }
                    }
                }
                child = child->next;
            }
        }

        xmlFreeDoc(doc);
    }
}

void vpi_config_free()
{
    free(vpi_config.connected_console_entries);
}

int vpi_config_add_console(vpi_console_entry_t *entry)
{
    if (vpi_config.connected_console_count == UINT8_MAX) {
        // Nothing to do
        return -1;
    }

    vpi_console_entry_t *old_entries = vpi_config.connected_console_entries;
    vpi_config.connected_console_entries = malloc((vpi_config.connected_console_count + 1) * sizeof(vpi_console_entry_t));
    if (vpi_config.connected_console_count > 0)
        memcpy(vpi_config.connected_console_entries, old_entries, vpi_config.connected_console_count * sizeof(vpi_console_entry_t));

    int index = vpi_config.connected_console_count;

    memcpy(vpi_config.connected_console_entries + vpi_config.connected_console_count, entry, sizeof(vpi_console_entry_t));
    vpi_config.connected_console_count++;
    if (old_entries)
        free(old_entries);

    vpi_config_save();

    return index;
}

void vpi_config_rename_console(uint8_t index, const char *name)
{
    vui_strncpy(vpi_config.connected_console_entries[index].name, name, VPI_CONSOLE_MAX_NAME);
    vpi_config_save();
}

void vpi_config_remove_console(uint8_t index)
{
    if (index >= vpi_config.connected_console_count) {
        // Nothing to do
        return;
    }

    vpi_console_entry_t *old_entries = vpi_config.connected_console_entries;

    if (vpi_config.connected_console_count != 1) {
        vpi_config.connected_console_entries = malloc((vpi_config.connected_console_count - 1) * sizeof(vpi_console_entry_t));

        // Copy first half
        memcpy(vpi_config.connected_console_entries, old_entries, index * sizeof(vpi_console_entry_t));

        // Copy second half
        memcpy(vpi_config.connected_console_entries + index, old_entries + index + 1, (vpi_config.connected_console_count - index - 1) * sizeof(vpi_console_entry_t));
    }

    vpi_config.connected_console_count--;

    free(old_entries);

    vpi_config_save();
}

void vpi_config_reset_default_controls()
{
    vpi_config_reset_default_controls_internal();
    vpi_config_save();
}
