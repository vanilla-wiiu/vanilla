#ifndef KEYMAP_H
#define KEYMAP_H

#include <QObject>
#include <map>

class KeyMap : public std::map<Qt::Key, int>
{
public:
    KeyMap();

    bool load(const QString &filename);
    bool save(const QString &filename);
    
    static QString getConfigFilename();

    static KeyMap instance;

};

#endif