#include <cstdlib>

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QRasterWindow>
#include <QThread>

#include "display.hpp"

namespace csim {

namespace {

/// @brief Draws the image scaled to the window.
class ImageWindow : public QRasterWindow {
public:
  void setImage(const QImage &image)
  {
    if (image.size() != image_.size()) resize(image.size());
    image_ = image;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.drawImage(QRect(QPoint(0, 0), size()), image_);
  }

private:
  QImage image_;
};

/// @brief Without a display, QGuiApplication would abort the process.
bool haveDisplay()
{
#if defined(__APPLE__) || defined(_WIN32)
  return true;
#else
  return (std::getenv("DISPLAY") != nullptr) || (std::getenv("WAYLAND_DISPLAY") != nullptr) ||
         (std::getenv("QT_QPA_PLATFORM") != nullptr);
#endif
}

/// @brief True if the window can be opened: the application uses no Qt, or
/// a QGuiApplication on this thread.
bool canOpen()
{
  const QCoreApplication *app = QCoreApplication::instance();
  if (app == nullptr) return haveDisplay();
  return (qobject_cast<const QGuiApplication *>(app) != nullptr) &&
         (app->thread() == QThread::currentThread());
}

/**
 * @brief Opens the window, and the Qt application if the program has none.
 *
 * Neither is ever destroyed. The process can end at any point, and Qt
 * destroys its own static data at exit, before the application if that is a
 * static object as well.
 */
ImageWindow *open(QSize size)
{
  if (QCoreApplication::instance() == nullptr) {
    static int argc = 1;
    static char name[] = "csim";
    static char *argv[] = {name, nullptr};
    new QGuiApplication(argc, argv);
  }
  auto *window = new ImageWindow;
  window->setTitle("CSIM - 3dfx Napalm");
  window->resize(size);
  window->show();
  return window;
}

}  // namespace

void showImage(const unsigned int *pixels, FxU32 width, FxU32 height)
{
  static const bool enabled = windowEnabled() && canOpen();
  if (!enabled || (width == 0) || (height == 0)) return;

  const QImage image = QImage(reinterpret_cast<const uchar *>(pixels), static_cast<int>(width),
                              static_cast<int>(height), QImage::Format_RGB32)
                           .copy();
  static ImageWindow *window = open(image.size());
  if (!window->isVisible()) return;  // closed

  window->setImage(image);
  QCoreApplication::processEvents();
}

}  // namespace csim
