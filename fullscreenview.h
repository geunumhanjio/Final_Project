/**
 * @file fullscreenview.h
 * @brief 전체 화면 확대 뷰 헤더
 */
#ifndef FULLSCREENVIEW_H
#define FULLSCREENVIEW_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>

class FullScreenView : public QWidget
{
    Q_OBJECT
public:
    explicit FullScreenView(QWidget *parent = nullptr);
    void setContent(int index); // 채널 내용 설정

signals:
    void closeRequested(); // 닫기 요청 시그널

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QLabel *screenLabel;
    QPushButton *btnClose;
    QString getChannelName(int index);
};

#endif // FULLSCREENVIEW_H
