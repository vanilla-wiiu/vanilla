#include "keymap.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QXmlStreamWriter>
#include <vanilla.h>

KeyMap KeyMap::instance;

KeyMap::KeyMap()
{
    KeyMap &ref = *this;
    ref[Qt::Key_Up] = VANILLA_BTN_UP;
    ref[Qt::Key_Down] = VANILLA_BTN_DOWN;
    ref[Qt::Key_Left] = VANILLA_BTN_LEFT;
    ref[Qt::Key_Right] = VANILLA_BTN_RIGHT;
    ref[Qt::Key_Z] = VANILLA_BTN_A;
    ref[Qt::Key_X] = VANILLA_BTN_B;
    ref[Qt::Key_C] = VANILLA_BTN_X;
    ref[Qt::Key_V] = VANILLA_BTN_Y;
    ref[Qt::Key_Return] = VANILLA_BTN_PLUS;
    ref[Qt::Key_Control] = VANILLA_BTN_MINUS;
    ref[Qt::Key_H] = VANILLA_BTN_HOME;
    ref[Qt::Key_W] = VANILLA_AXIS_L_UP;
    ref[Qt::Key_A] = VANILLA_AXIS_L_LEFT;
    ref[Qt::Key_S] = VANILLA_AXIS_L_DOWN;
    ref[Qt::Key_D] = VANILLA_AXIS_L_RIGHT;
    ref[Qt::Key_E] = VANILLA_BTN_L3;
    ref[Qt::Key_8] = VANILLA_AXIS_R_UP;
    ref[Qt::Key_4] = VANILLA_AXIS_R_LEFT;
    ref[Qt::Key_2] = VANILLA_AXIS_R_DOWN;
    ref[Qt::Key_6] = VANILLA_AXIS_R_RIGHT;
    ref[Qt::Key_5] = VANILLA_BTN_R3;
    ref[Qt::Key_T] = VANILLA_BTN_L;
    ref[Qt::Key_G] = VANILLA_BTN_ZL;
    ref[Qt::Key_U] = VANILLA_BTN_R;
    ref[Qt::Key_J] = VANILLA_BTN_ZR;
}

int g_serializationVersion = 1;

bool KeyMap::load(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        return false;
    }

    this->clear();

    QXmlStreamReader reader(&file);

    while (reader.readNextStartElement()) {
        if (reader.name() == QStringLiteral("VanillaInputMap")) {
            for (auto attr : reader.attributes()) {
                if (attr.name() == QStringLiteral("Version")) {
                    if (attr.value().toInt() != g_serializationVersion) {
                        return false;
                    }
                }
            }

            while (reader.readNextStartElement()) {
                if (reader.name() == QStringLiteral("Key")) {
                    int vanillaBtn = -1;
                    for (auto attr : reader.attributes()) {
                        if (attr.name() == QStringLiteral("Id")) {
                            vanillaBtn = attr.value().toInt();
                        }
                    }

                    if (vanillaBtn != -1) {
                        int qtKey = reader.readElementText().toInt();
                        (*this)[static_cast<Qt::Key>(qtKey)] = vanillaBtn;
                    } else {
                        reader.skipCurrentElement();
                    }
                } else {
                    reader.skipCurrentElement();
                }
            }
        } else {
            reader.skipCurrentElement();
        }
    }

    return true;
}

bool KeyMap::save(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::WriteOnly)) {
        return false;
    }

    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);

    writer.writeStartDocument();

    writer.writeStartElement(QStringLiteral("VanillaInputMap"));

    writer.writeAttribute(QStringLiteral("Version"), QString::number(g_serializationVersion));

    for (auto it = this->cbegin(); it != this->cend(); it++) {
        writer.writeStartElement(QStringLiteral("Key"));
        writer.writeAttribute(QStringLiteral("Id"), QString::number(it->second));
        writer.writeCharacters(QString::number(it->first));
        writer.writeEndElement(); // Key
    }

    writer.writeEndElement(); // VanillaInputMap

    writer.writeEndDocument();

    file.close();
    
    return true;
}

QString KeyMap::getConfigFilename()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation));
    dir = dir.filePath("vanilla");
    dir.mkpath(".");
    return dir.filePath("keymap.xml");
}