/*
Copyright (c) 2014, Daniel Moreno and Gabriel Taubin
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Brown University nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL DANIEL MORENO AND GABRIEL TAUBIN BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "CaptureDialog.hpp"

#include <QDesktopWidget>
#include <QMessageBox>
#include <QTimer>
#include <QTime>

#include <iostream>
#include <opencv2/imgproc/imgproc.hpp>

#include "Application.hpp"
#include "im_util.hpp"
#include "pgcamera.hpp"

int  cameratype = 0; // 1: webcamera, 0:PG camera
CaptureDialog::CaptureDialog(QWidget * parent, Qt::WindowFlags flags): 
    QDialog(parent, flags),
    _projector(),
    _pg_camera(this),
    _video_input(this),
    _capture(false),
    _session(),
    _wait_time(0),
    _total(0),
    _cancel(false)
{
    setupUi(this);
    camera_resolution_label->clear();

    connect(APP, SIGNAL(root_dir_changed(const QString&)), this, SLOT(_on_root_dir_changed(const QString&)));

    current_message_label->setVisible(false);
    progress_label->setVisible(false);
    progress_bar->setVisible(false);

    update_screen_combo();
    update_camera_combo();

    projector_patterns_spin->setValue(APP->config.value("capture/pattern_count", 10).toInt());
    camera_exposure_spin->setMaximum(9999);
    camera_exposure_spin->setValue(APP->config.value("capture/exposure_time", 500).toInt());
    output_dir_line->setText(APP->get_root_dir());

    continuous_spin->setValue(APP->config.value("capture/continuous", 100).toInt());
    continuous_spin->setMaximum(9999);
    continuous_spin->setMinimum(10);//20160910   capturedialog.cpp第74行  continuous_spin->setMinimum(10)改为continuous_spin->setMinimum(5);

    size_t rotation = APP->config.value("capture/rotation", 0).toUInt();
    if (rotation==0) { rot_000_radio->setChecked(true); }
    if (rotation==90) { rot_090_radio->setChecked(true); }
    if (rotation==280) { rot_180_radio->setChecked(true); }
    if (rotation==270) { rot_270_radio->setChecked(true); }

    //test buttons
    test_prev_button->setEnabled(false);
    test_next_button->setEnabled(false);

    //update projector view
    _projector.set_screen(screen_combo->currentIndex());



    // //// pg_camera ///////////////////////////////////////
    if(cameratype==1)
    {
        start_camera();
    }
    else
    {
    QImage srcImage;
    //PGCamera* pg_camera = new PGCamera();
    //myCalibrate(new Calibration())
     unsigned int cameraNums = _pg_camera.connectCamera();
     if(cameraNums==0)
     {
         QMessageBox::critical(this, "Error", "PG Camera is not ready");
         return;
     }
     else
     {
         //start video preview
         start_camera();
         // //// pg_camera ///////////////////////////////////////

         _pg_camera.captureImage();

         QImage *showImage = new QImage;
         QImage srcImage;
         ImageToQImage(_pg_camera.imageCamera,srcImage);//将相机获得的图像转换为QImage类型

         bool image_rotate =false; // 20160712 for rotate image  ///20160829 for no rotate image

         if(image_rotate)
         {
            QMatrix leftmatrix;
            leftmatrix.rotate(90);
            *showImage = srcImage.scaled(camera_image->height(),
                                      camera_image->width(),
                                      Qt::KeepAspectRatio);
            camera_image->setPixmap(QPixmap::fromImage(*showImage).transformed(leftmatrix,Qt::SmoothTransformation));
         }
         else
         {
            *showImage = srcImage.scaled(camera_image->width(),
                                      camera_image->height(),
                                      Qt::KeepAspectRatio);
            camera_image->setPixmap(QPixmap::fromImage(*showImage));
         }
         camera_image->show();

           // //// pg_camera ///////////////////////////////////////
         /*
         _pg_camera.startCamera();
         _pg_camera.captureImage();
         //将相机获取到的Image格式的图片转换成为QImage
         ImageToQImage(_pg_camera.imageCamera,srcImage);
         //设置显示视频用的Label
        // QLabel *imageLabel;
        // imageLabel = new QLabel(this);
        // camera_image->addWidget(imageLabel);

         QString imageFileName = "/";
        // imageFileName.append(QString::number(i, 10));
         imageFileName.append("test.bmp");
         srcImage.save(imageFileName);
         //tmpImage.Save(imageFileName);//  .save(imageFileName);
         */
     }
    }


    // //// pg_camera ///////////////////////////////////////

}

CaptureDialog::~CaptureDialog()
{
    stop_camera();

    //save user selection
    QSettings & config = APP->config;
    int projector_screen = screen_combo->currentIndex();
    if (projector_screen>=0)
    {
        config.setValue("capture/projector_screen", projector_screen);
    }
    QString camera_name = camera_combo->currentText();
    if (!camera_name.isEmpty())
    {
        config.setValue("capture/camera_name", camera_name);
    }
    config.setValue("capture/pattern_count", projector_patterns_spin->value());
    config.setValue("capture/exposure_time", camera_exposure_spin->value());
    config.setValue("capture/continuous", continuous_spin->value());

    int rotation = 0;
    if (rot_000_radio->isChecked()) { rotation = 0; }
    if (rot_090_radio->isChecked()) { rotation = 90; }
    if (rot_180_radio->isChecked()) { rotation = 180; }
    if (rot_270_radio->isChecked()) { rotation = 270; }
    APP->config.setValue("capture/rotation", rotation);
}

void CaptureDialog::reset(void)
{
    progress_bar->setMaximum(0);
    current_message_label->clear();
    close_cancel_button->setText("Cancel");
    _cancel = false;
}

void CaptureDialog::finish(void)
{
    //progress_bar->setValue(_total);
    close_cancel_button->setText("Close");
}

void CaptureDialog::on_close_cancel_button_clicked(bool checked)
{
    if (close_cancel_button->text()=="Close")
    {
        accept();
    }
    else if (!_cancel)
    {
        _cancel = true;
    }
}

int CaptureDialog::update_screen_combo(void)
{
    //disable signals
    screen_combo->blockSignals(true);

    //save current value
    int current = screen_combo->currentIndex();

    //update combo
    QStringList list;
    screen_combo->clear();
    QDesktopWidget * desktop = QApplication::desktop();
    int screens =  desktop->screenCount();
    for (int i=0; i<screens; i++)
    {
        const QRect rect = desktop->screenGeometry(i);
        list.append(QString("Screen %1 [%2x%3]").arg(i).arg(rect.width()).arg(rect.height()));
    }
    screen_combo->addItems(list);

    //set current value
    int saved_value = APP->config.value("capture/projector_screen", 1).toInt();
    int default_screen = (saved_value<screen_combo->count() ? saved_value : 0);
    screen_combo->setCurrentIndex((-1<current && current<screen_combo->count() ? current : default_screen));

    //enable signals
    screen_combo->blockSignals(false);

    return screen_combo->count();
}

int CaptureDialog::update_camera_combo(void)
{
    //disable signals
    camera_combo->blockSignals(true);

    //save current value
    QString current = camera_combo->currentText();

    //update combo
    camera_combo->clear();
    if( cameratype == 1)
    {
        camera_combo->addItems(_video_input.list_devices());
    }
    else //PG camera
    {
        camera_combo->addItems(_pg_camera.list_devices());
    }
    //set current value
    int index = camera_combo->findText(current);
    if (index<0)
    {
        QString saved_value = APP->config.value("capture/camera_name").toString();
        index = camera_combo->findText(saved_value);
    }
    camera_combo->setCurrentIndex((index<0 ? 0 : index));

    //enable signals
    camera_combo->blockSignals(false);

    return camera_combo->count();
}

void CaptureDialog::ImageToQImage(Image image,QImage &qImage)  //将相机捕获到的Image格式的图片转换成QImage
{
    //由于两者之间的属性定义相差太大，所以只好先转换成流文件，再用QImage读进来
    ostringstream filename;
    filename << "captureImage" << ".bmp";
    image.Save( filename.str().c_str() );//将获得的图像流存储并实时刷新
    qImage.load( QString::fromStdString(filename.str()));
	//qImage.load( QString::fromStdString(filename.str().c_str()));
}



bool CaptureDialog::start_camera(void)
{
    int index = camera_combo->currentIndex();
    if(cameratype==1)
    {
        if (_video_input.get_camera_index()==index)
        {
            return true;
        }
    }
    else
    {
        if (_pg_camera.get_camera_index()==index)
        {
            return true;
        }
    }

    //busy cursor
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QApplication::processEvents();

    if(cameratype==1)
    {
        stop_camera();
        _video_input.start();
        _video_input.set_camera_index(index);
        _video_input.start();
        _video_input.waitForStart();

        if (!_video_input.isRunning())
        {   //error
            //TODO: display error --------
            camera_resolution_label->clear();
            camera_resolution_label->setContextMenuPolicy(Qt::NoContextMenu);
        }
    }
    else
    {
        _pg_camera.start();
        _pg_camera.set_camera_index(index);
        _pg_camera.waitForStart();

        if (!_pg_camera.isRunning())
        {   //error
            //TODO: display error --------
            camera_resolution_label->clear();
            camera_resolution_label->setContextMenuPolicy(Qt::NoContextMenu);
        }
        //_pg_camera.startCamera();

    }

    // connect display signal now that we know it worked
    QStringList resolutionList;
    if(cameratype==1)
    {
        connect(&_video_input, SIGNAL(new_image(cv::Mat)), this, SLOT(_on_new_camera_image(cv::Mat)), Qt::DirectConnection);

    //setup resolutions context menu
         resolutionList = _video_input.list_device_resolutions(index);
    }
    else
    {
        connect(&_pg_camera, SIGNAL(new_image(cv::Mat)), this, SLOT(_on_new_camera_image(cv::Mat)), Qt::DirectConnection);

    //setup resolutions context menu
       // resolutionList = _video_input.list_device_resolutions(index);

    }
    if (!resolutionList.empty())
    {
        //clean previous actions
        QList<QAction *> actions = camera_resolution_label->actions();
        foreach(auto action, actions)
        {
            camera_resolution_label->removeAction(action);
            delete action;
        }

        //add new actions
        foreach (auto res, resolutionList)
        {
            QAction * action = new QAction(res, this);
            connect(action, SIGNAL(triggered(bool)), this, SLOT(_on_resolution_change()));
            camera_resolution_label->addAction(action);
        }
        camera_resolution_label->setContextMenuPolicy(Qt::ActionsContextMenu);
    }


    //restore regular cursor
    QApplication::restoreOverrideCursor();
    QApplication::processEvents();

    return true;
}

void CaptureDialog::stop_camera(void)
{
    //disconnect display signal first
    if(cameratype==1){
        disconnect(&_video_input, SIGNAL(new_image(cv::Mat)), this, SLOT(_on_new_camera_image(cv::Mat)));
    }
    else{
        disconnect(&_pg_camera, SIGNAL(new_image(cv::Mat)), this, SLOT(_on_new_camera_image(cv::Mat)));
    }
    //clean up
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor)); //busy cursor
    QApplication::processEvents(); //process pending signals
    camera_image->clear();
    camera_resolution_label->clear();
    
    if(cameratype==1){
    //stop the thread
        if (_video_input.isRunning())
        {
            _video_input.stop();
            _video_input.wait();
        }
    }
    else
    {
        if (_pg_camera.isRunning())
        {
            _pg_camera.stop();
            _pg_camera.wait();
        }
        //_pg_camera.stopCamera();
    }
    //restore regular cursor
    QApplication::restoreOverrideCursor();
    QApplication::processEvents();
}

void CaptureDialog::_on_root_dir_changed(const QString & dirname)
{
    output_dir_line->setText(dirname);
}

void CaptureDialog::_on_new_camera_image(cv::Mat image)
{
  size_t rotation = 0;
  if (rot_000_radio->isChecked()) { rotation = 0; }
  if (rot_090_radio->isChecked()) { rotation = 90; }
  if (rot_180_radio->isChecked()) { rotation = 180; }
  if (rot_270_radio->isChecked()) { rotation = 270; }
  if (rotation>0)
  {
    image = im_util::rotate_image(image, rotation);
  }

    camera_image->setImage(image);

//    camera_image->show();
    camera_resolution_label->setText(QString("[%1x%2]").arg(image.cols).arg(image.rows));

    if (_capture)
    {   //save this image
       /* bool image_rotate =true; // 20160712 for rotate image
        if(image_rotate)
        {
            cv::Mat dst;
            transpose(image, dst);  flip(dst, image, 1); // transpose + flip by y axis = rotate 90 clockwise
            // transpose(image, dst);  flip(dst, image, 0); // transpose + flip by x axis = rotate 90 counter-clockwise
        }
        */
        cv::Mat tran_img,rotated_img;
        transpose(image, tran_img);//将图像顺时针选择90度
        flip(tran_img,rotated_img,1);//将旋转后的图像按y轴方向沿中心翻转

        camera_image->setImage(rotated_img);
        cv::imwrite(QString("%1/cam_%2.png").arg(_session).arg(_projector.get_current_pattern() + 1, 2, 10, QLatin1Char('0')).toStdString(), /*rotated_img*/image);//20160917  
        _capture = false;
        _projector.clear_updated();
    }
    else
    {
        if(cameratype!=1) // for display
        {
            _pg_camera.captureImage();

            QImage *showImage = new QImage;
            QImage srcImage;
            ImageToQImage(_pg_camera.imageCamera,srcImage);
            bool image_rotate =false; // 20160712 for rotate image   20160917 for non-rotate image
            if(image_rotate)
            {
               QMatrix leftmatrix;
               leftmatrix.rotate(90);
               *showImage = srcImage.scaled(camera_image->height(),
                                         camera_image->width(),
                                         Qt::KeepAspectRatio);
               camera_image->setPixmap(QPixmap::fromImage(*showImage).transformed(leftmatrix,Qt::SmoothTransformation));
            }
            else
            {
               *showImage = srcImage.scaled(camera_image->width(),
                                         camera_image->height(),
                                         Qt::KeepAspectRatio);
               camera_image->setPixmap(QPixmap::fromImage(*showImage));
            }
            camera_image->show();
        }

    }
}

void CaptureDialog::_on_resolution_change()
{
    QAction * action = qobject_cast<QAction *>(sender());
    if (!action) { return; }

    QStringList res = action->text().split('x');
    if (res.length()<2) { return; }

    unsigned int cols = res.at(0).toUInt();
    unsigned int rows = res.at(1).toUInt();
    _video_input.setImageSize(cols, rows);
}

void CaptureDialog::_on_new_projector_image(QPixmap image)
{
     projector_image->setPixmap(image);
	//_projector.setPixmap(image);//显示在界面上的条纹图片
}

void CaptureDialog::on_screen_combo_currentIndexChanged(int index)
{
    _projector.set_screen(index);
}

void CaptureDialog::on_camera_combo_currentIndexChanged(int index) 
{
    camera_resolution_label->clear();
    start_camera();
}

void CaptureDialog::on_output_dir_line_textEdited(const QString & text)
{
    APP->set_root_dir(text);
}

void CaptureDialog::on_output_dir_button_clicked(bool checked)
{
    APP->change_root_dir(this);
}

void CaptureDialog::wait(int msecs)
{
    QTime timer;
    timer.start();
    while (timer.elapsed()<msecs)
    {   
        QApplication::processEvents();
    }
}

void CaptureDialog::on_capture_button_clicked(bool checked=true)//20161103 bool checked->bool checked=true
{
    //start_camera();

    //check camera
    if(cameratype==1)
    {
        if (!_video_input.isRunning())
        {
            QMessageBox::critical(this, "Error", "WebCamera is not ready");
            return;
        }
    }
    else
    {
        if (!_pg_camera.isRunning())
        {
            QMessageBox::critical(this, "Error", "PGCamera is not ready");
            return;
        }
    }


    //make output dir
    _session = APP->get_root_dir()+"/"+QDateTime::currentDateTime().toString("yyyy-dd_hh.mm.ss.zzz");
    QDir session_dir;
    if (!session_dir.mkpath(_session))
    {
        QMessageBox::critical(this, "Error", "Cannot create output directory:\n"+_session);
        std::cout << "Failed to create directory: " << _session.toStdString() << std::endl;
        return;
    }

    //disable GUI interaction
    projector_group->setEnabled(false);
    camera_group->setEnabled(false);
    other_group->setEnabled(false);
    capture_button->setEnabled(false);
    close_cancel_button->setEnabled(false);

    _capture = false;
    _wait_time = camera_exposure_spin->value();

    //connect projector display signal
    connect(&_projector, SIGNAL(new_image(QPixmap)), this, SLOT(_on_new_projector_image(QPixmap)));

    //open projector
    _projector.set_pattern_count(projector_patterns_spin->value());
	
    _projector.start();

    //save projector resolution and settings
    size_t rotation = 0;
    if (rot_000_radio->isChecked()) { rotation = 0; }
    if (rot_090_radio->isChecked()) { rotation = 90; }
    if (rot_180_radio->isChecked()) { rotation = 180; }
    if (rot_270_radio->isChecked()) { rotation = 270; }
    _projector.save_info(QString("%1/projector_info.txt").arg(_session), (rotation==90||rotation==270?true:false));

    //init time
    wait(_wait_time);

    while (!_projector.finished())
    {
        _projector.next();
        //QMessageBox::critical(this, "Next", "Continue");

        //wait for projector
        while (!_projector.is_updated())
        {   
            QApplication::processEvents();
        }

        //pause so the screen gets updated
        wait(_wait_time);

        //capture
        _capture = true;

        //wait for camera
		
        while (_capture)
        {   
            QApplication::processEvents();
        }
	
    }

    //close projector
    _projector.stop();

    //disconnect projector display signal
    disconnect(&_projector, SIGNAL(new_image(QPixmap)), this, SLOT(_on_new_projector_image(QPixmap)));
   

    //re-read images
    APP->set_root_dir(APP->get_root_dir());

    //enable GUI interaction
    projector_group->setEnabled(true);
    camera_group->setEnabled(true);
    other_group->setEnabled(true);    
    capture_button->setEnabled(true);
    close_cancel_button->setEnabled(true);



    //TODO: override window close button or allow to close/cancel while capturing
}

void CaptureDialog::on_test_check_stateChanged(int state)
{
    //adjust the GUI
    bool checked = (state==Qt::Checked);
    test_prev_button->setEnabled(checked);
    test_next_button->setEnabled(checked);
    capture_button->setEnabled(!checked);
    screen_combo->setEnabled(!checked);
    projector_patterns_spin->setEnabled(!checked);

    if (checked)
    {   //start preview

        //connect projector display signal
        connect(&_projector, SIGNAL(new_image(QPixmap)), this, SLOT(_on_new_projector_image(QPixmap)));

        //open projector
        _projector.set_pattern_count(projector_patterns_spin->value());
        _projector.start();
        _projector.next();

        //continuous
        if (continuous_check->isChecked())
        {
            QTimer::singleShot(continuous_spin->value(), this, SLOT(auto_next()));
        }
    }
    else
    {   //stop preview

        //close projector
        _projector.stop();

        //disconnect projector display signal
        disconnect(&_projector, SIGNAL(new_image(QPixmap)), this, SLOT(_on_new_projector_image(QPixmap)));
    }
}

void CaptureDialog::on_test_prev_button_clicked(bool checked)
{
    _projector.clear_updated();
    _projector.prev();
}

void CaptureDialog::on_test_next_button_clicked(bool checked)
{
    _projector.clear_updated();
    _projector.next();
}

void CaptureDialog::auto_next()
{
    if (test_check->isChecked() && continuous_check->isChecked())
    {
        _projector.clear_updated();
        
        if (_projector.finished())
        {
            _projector.start();
        }

        _projector.next();

        QTimer::singleShot(continuous_spin->value(), this, SLOT(auto_next()));

        QApplication::processEvents();
    }
}
