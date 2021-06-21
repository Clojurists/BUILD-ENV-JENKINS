#include "macdockiconhandler.h"

#include <QImageWriter>
#include <QMenu>
#include <QTemporaryFile>
#include <QWidget>

#undef slots
#include <Cocoa/Cocoa.h>

#if QT_VERSION < 0x050000
extern void qt_mac_set_dock_menu(QMenu *);
#endif

@interface DockIconClickEventHandler : NSObject
{
    MacDockIconHandler* dockIconHandler;
}

@end

@implementation DockIconClickEventHandler

- (id)initWithDockIconHandler:(MacDockIconHandler *)aDockIconHandler
{
    self = [super init];
    if (self) {
        dockIconHandler = aDockIconHandler;

        [[NSAppleEventManager sharedAppleEventManager]
            setEventHandler:self
                andSelector:@selector(handleDockClickEvent:withReplyEvent:)
              forEventClass:kCoreEventClass
                 andEventID:kAEReopenApplication];
    }
    return self;
}

- (void)handleDockClickEvent:(NSAppleEventDescriptor*)event withReplyEvent:(NSAppleEventDescriptor*)replyEvent
{
    Q_UNUSED(event)
    Q_UNUSED(replyEvent)

    if (dockIconHandler) {
        dockIconHandler->handleDockIconClickEvent();
    }
}

@end

MacDockIconHandler::MacDockIconHandler() : QObject()
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    this->m_dockIconClickEventHandler = [[DockIconClickEventHandler alloc] initWithDockIconHandler:this];
    this->m_dummyWidget = new QWidget();
    this->m_dockMenu = new QMenu(this->m_dummyWidget);
    this->setMainWindow(NULL);
#if QT_VERSION < 0x050000
    qt_mac_set_dock_menu(this->m_dockMenu);
#endif
    [pool release];
}

void MacDockIconHandler::setMainWindow(QMainWindow *window) {
    this->mainWindow = window;
}

MacDockIconHandler::~MacDockIconHandler()
{
    [this->m_dockIconClickEventHandler release];
    delete this->m_dummyWidget;
    this->setMainWindow(NULL);
}

QMenu *MacDockIconHandler::dockMenu()
{
    return this->m_dockMenu;
}

void MacDockIconHandler::setIcon(const QIcon &icon)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSImage *image = nil;
    if (icon.isNull()) 
    {
        image = [[NSImage imageNamed:@"NSApplicationIcon"] retain];
    }
    else 
    {
        QSize size = icon.actualSize(QSize(128, 128));
        QPixmap pixmap = icon.pixmap(size);

//      CGImageRef cgImage = pixmap.toMacCGImageRef();
//      image = [[NSImage alloc] initWithCGImage:cgImage size:NSZeroSize];
//      CFRelease(cgImage);

        // write temp file DRM (could also be done through QIODevice [memory])
        QTemporaryFile notificationIconFile;
        if (!pixmap.isNull() && notificationIconFile.open()) {
            QImageWriter writer(&notificationIconFile, "PNG");
            if (writer.write(pixmap.toImage())) {
                const char *cStri