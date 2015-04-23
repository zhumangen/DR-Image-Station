#include "studydbmanager.h"
#include "../share/global.h"
#include "../share/studyrecord.h"
#include "../MainStation/mainwindow.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QFile>
#include <QDir>
#include <QSqlRecord>

QString StudyDbManager::lastError;

bool StudyDbManager::createStudyDb(bool recreate)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", STUDY_DB_CONNECTION_NAME);
    db.setDatabaseName(STUDY_DB_NAME);
    if (db.open()) {
        if (recreate) {
            db.exec("DROP INDEX IX_StudyTable_StudyDate ON StudyTable");
            db.exec("DROP TABLE StudyTable");
            db.exec("DROP INDEX IX_ImageTable_ImageTime ON ImageTable");
            db.exec("DROP TABLE ImageTable");
            db.exec("DROP INDEX IX_ReportTable_CreateTime ON ReportTable");
            db.exec("DROP TABLE ReportTable");
        }
        db.exec("CREATE TABLE IF NOT EXISTS StudyTable(StudyUid VARCHAR(128) PRIMARY KEY NOT NULL,"
                "AccNumber VARCHAR(64) NOT NULL, PatientId VARCHAR(64) NOT NULL,"
                "PatientName VARCHAR(64), PatientSex VARCHAR(2) NOT NULL,"
                "PatientBirth DATE NOT NULL, PatientAge VARCHAR(6), StudyTime DATETIME NOT NULL,"
                "Modality VARCHAR(2) NOT NULL, IsAcquisited VARCHAR(6), IsReported VARCHAR(6),"
                "IsPrinted VARCHAR(6), IsSent VARCHAR(6), StudyDesc TEXT,"
                "ReqPhysician VARCHAR(64), PerPhysician VARCHAR(64),"
                "MedicalAlert TEXT, PatientSize VARCHAR(6), PatientWeight VARCHAR(6),"
                "PatientAddr TEXT, PatientPhone VARCHAR(16))");
        db.exec("CREATE INDEX IF NOT EXISTS IX_StudyTable_StudyDate ON StudyTable(StudyTime)");
        db.exec("CREATE TABLE IF NOT EXISTS ImageTable(ImageUid VARCHAR(128) PRIMARY KEY NOT NULL,"
                "SopClassUid VARCHAR(128) NOT NULL,"
                "SeriesUid VARCHAR(128) NOT NULL, StudyUid VARCHAR(128) NOT NULL,"
                "RefImageUid VARCHAR(128),"
                "ImageNo VARCHAR(16), ImageTime DATETIME NOT NULL,"
                "BodyPart VARCHAR(128), IsPrinted VARCHAR(6), IsSent VARCHAR(6),"
                "ImageDesc TEXT, ImageFile TEXT,"
                "FOREIGN KEY(StudyUid) REFERENCES StudyTable(StudyUid))");
        db.exec("CREATE INDEX IF NOT EXISTS IX_ImageTable_ImageTime ON ImageTable(ImageTime)");
        db.exec("CREATE TABLE IF NOT EXISTS ReportTable(ReportUid VARCHAR(128) PRIMARY KEY NOT NULL,"
                "SeriesUid VARCHAR(128) NOT NULL, StudyUid VARCHAR(128) NOT NULL,"
                "CreateTime DATETIME NOT NULL, ContentTime DATETIME NOT NULL,"
                "IsCompleted VARCHAR(6), IsVerified VARCHAR(6), IsPrinted VARCHAR(6),"
                "ReportFile TEXT,"
                "FOREIGN KEY(StudyUid) REFERENCES StudyTable(StudyUid))");
        db.exec("CREATE INDEX IF NOT EXISTS IX_ReportTable_CreateTime ON ReportTable(CreateTime)");
    }

    lastError = db.lastError().text();
    return db.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::insertStudyToDb(const StudyRecord &study)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare(QString("INSERT INTO StudyTable VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(study.studyUid);
    query.addBindValue(study.accNumber);
    query.addBindValue(study.patientId);
    query.addBindValue(study.patientName);
    query.addBindValue(study.patientSex);
    query.addBindValue(study.patientBirth.toString("yyyy-MM-dd"));
    query.addBindValue(study.patientAge);
    query.addBindValue(study.studyTime.toString("yyyy-MM-dd hh:mm:ss"));
    query.addBindValue(study.modality);

    query.addBindValue(QObject::tr("No"));
    query.addBindValue(QObject::tr("No"));
    query.addBindValue(QObject::tr("No"));
    query.addBindValue(QObject::tr("No"));

    query.addBindValue(study.studyDesc);
    query.addBindValue(study.reqPhysician);
    query.addBindValue(study.perPhysician);

    query.addBindValue(study.medicalAlert);
    query.addBindValue(study.patientSize);
    query.addBindValue(study.patientWeight);
    query.addBindValue(study.patientAddr);
    query.addBindValue(study.patientPhone);

    query.exec();
    QString err = lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::removeStudyFromDb(const QString &studyUid)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("SELECT RefImageUid, ImageFile FROM ImageTable WHERE StudyUid=?");
    query.addBindValue(studyUid);
    query.exec();
    while (query.next()) {
        QSqlRecord rec = query.record();
        QString refUid = rec.value(0).toString();
        QString file = QString("%1/%2").arg(mainWindow->getDbLocation(), rec.value(1).toString());
        QFile(file).remove();
        QString dirName = file.left(file.lastIndexOf('/'));
        if (!refUid.isEmpty()){
            QString rawFile = QString("%1/%2_%3.dcm").arg(dirName, QString(RAW_IMAGE_PREFIX), refUid);
            QFile(rawFile).remove();
        }
        QDir().rmpath(dirName);
    }
    query.prepare("DELETE FROM ImageTable WHERE StudyUid=?");
    query.addBindValue(studyUid);
    query.exec();

    query.prepare("SELECT ReportFile FROM ReportTable WHERE StudyUid=?");
    query.addBindValue(studyUid);
    query.exec();
    while (query.next()) {
        QSqlRecord rec = query.record();
        QString file = QString("%1/%2").arg(mainWindow->getDbLocation(), rec.value(0).toString());
        QFile(file).remove();
        QDir().rmpath(file.left(file.lastIndexOf('/')));
    }
    query.prepare("DELETE FROM ReportTable WHERE StudyUid=?");
    query.addBindValue(studyUid);
    query.exec();

    query.prepare("DELETE FROM StudyTable WHERE StudyUid=?");
    query.addBindValue(studyUid);
    query.exec();
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::updateStudyAcquisitStatus(const QString &studyUid, bool acquisited)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("UPDATE StudyTable SET IsAcquisited=? WHERE StudyUid=?");
    query.addBindValue(acquisited?QObject::tr("Yes"):QObject::tr("No"));
    query.addBindValue(studyUid);
    query.exec();
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::updateStudyPrintStatus(const QString &studyUid, int printed)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("UPDATE StudyTable SET IsPrinted=? WHERE StudyUid=?");

    switch (printed) {
    case 0:
        query.addBindValue(QObject::tr("No"));
        break;
    case 1:
        query.addBindValue(QObject::tr("Partial"));
        break;
    case 2:
        query.addBindValue(QObject::tr("Yes"));
        break;
    default:
        query.addBindValue(QString());
        break;
    }

    query.addBindValue(studyUid);
    query.exec();
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::updateStudyReportStatus(const QString &studyUid, bool reported)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("UPDATE StudyTable SET IsReported=? WHERE StudyUid=?");
    query.addBindValue(reported?QObject::tr("Yes"):QObject::tr("No"));
    query.addBindValue(studyUid);
    query.exec();
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::updateStudySendStatus(const QString &studyUid, int sent)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("UPDATE StudyTable SET IsSent=? WHERE StudyUid=?");

    switch (sent) {
    case 0:
        query.addBindValue(QObject::tr("No"));
        break;
    case 1:
        query.addBindValue(QObject::tr("Partial"));
        break;
    case 2:
        query.addBindValue(QObject::tr("Yes"));
        break;
    default:
        query.addBindValue(QString());
        break;
    }

    query.addBindValue(studyUid);
    query.exec();
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::insertImageToDb(const ImageRecord &image)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare(QString("INSERT INTO ImageTable VALUES(?,?,?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(image.imageUid);
    query.addBindValue(image.sopClassUid);
    query.addBindValue(image.seriesUid);
    query.addBindValue(image.studyUid);
    query.addBindValue(image.refImageUid);
    query.addBindValue(image.imageNo);
    query.addBindValue(image.imageTime.toString("yyyy-MM-dd hh:mm:ss"));
    query.addBindValue(image.bodyPart);
    query.addBindValue(QObject::tr("No"));
    query.addBindValue(QObject::tr("No"));
    query.addBindValue(image.imageDesc);
    query.addBindValue(image.imageFile);
    if (query.exec()) updateStudyAcquisitStatus(image.studyUid, true);
    QString err = lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::removeImageFromDb(const QString &imageUid)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("SELECT RefImageUid, ImageFile FROM ImageTable WHERE ImageUid=?");
    query.addBindValue(imageUid);
    query.exec();
    while (query.next()) {
        QSqlRecord rec = query.record();
        QString refUid = rec.value(0).toString();
        QString file = QString("%1/%2").arg(mainWindow->getDbLocation(), rec.value(1).toString());
        QFile(file).remove();
        QString dirName = file.left(file.lastIndexOf('/'));
        if (!refUid.isEmpty()){
            QString rawFile = QString("%1/%2_%3.dcm").arg(dirName, QString(RAW_IMAGE_PREFIX), refUid);
            QFile(rawFile).remove();
        }
        QDir().rmpath(dirName);
    }
    query.prepare("DELETE FROM ImageTable WHERE ImageUid=?");
    query.addBindValue(imageUid);
    query.exec();
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::insertReportToDb(const ReportRecord &report)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare(QString("INSERT INTO ReportTable VALUES(?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(report.reportUid);
    query.addBindValue(report.seriesUid);
    query.addBindValue(report.studyUid);
    query.addBindValue(report.createTime.toString("yyyy-MM-dd hh:mm:ss"));
    query.addBindValue(report.contentTime.toString("yyyy-MM-dd hh:mm:ss"));
    query.addBindValue(report.isCompleted?QObject::tr("Yes"):QObject::tr("No"));
    query.addBindValue(report.isVerified?QObject::tr("Yes"):QObject::tr("No"));
    query.addBindValue(QObject::tr("No"));
    query.addBindValue(report.reportFile);
    if (query.exec()) updateStudyReportStatus(report.studyUid, true);
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::updateImagePrintStatus(const QString &imageUid, bool printed)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("UPDATE ImageTable SET IsPrinted=? WHERE ImageUid=?");
    query.addBindValue(printed?QObject::tr("Yes"):QObject::tr("No"));
    query.addBindValue(imageUid);
    if (query.exec()) {
        query.prepare("SELECT StudyUid FROM ImageTable WHERE ImageUid=?");
        query.addBindValue(imageUid);
        query.exec();
        while (query.next()) {
            QString studyUid = query.record().value(0).toString();
            query.prepare("SELECT IsPrinted FROM ImageTable WHERE StudyUid=?");
            query.addBindValue(studyUid);
            query.exec();
            bool allPrinted = true;
            bool nonePrinted = true;
            while (query.next()) {
                QString status = query.record().value(0).toString();
                if ((status == QObject::tr("Yes")) || (status == "Yes")) {
                    nonePrinted = false;
                } else {
                    allPrinted = false;
                }
            }
            updateStudyPrintStatus(studyUid, allPrinted?2:(nonePrinted?0:1));
        }
    }
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::updateImageSendStatus(const QString &imageUid, bool sent)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("UPDATE ImageTable SET IsSent=? WHERE ImageUid=?");
    query.addBindValue(sent?QObject::tr("Yes"):QObject::tr("No"));
    query.addBindValue(imageUid);
    if (query.exec()) {
        query.prepare("SELECT StudyUid FROM ImageTable WHERE ImageUid=?");
        query.addBindValue(imageUid);
        query.exec();
        while (query.next()) {
            QString studyUid = query.record().value(0).toString();
            query.prepare("SELECT IsSent FROM ImageTable WHERE StudyUid=?");
            query.addBindValue(studyUid);
            query.exec();
            bool allSent = true;
            bool noneSent = true;
            while (query.next()) {
                QString status = query.record().value(0).toString();
                if ((status == QObject::tr("Yes")) || (status == "Yes")) {
                    noneSent = false;
                } else {
                    allSent = false;
                }
            }
            updateStudySendStatus(studyUid, allSent?2:(noneSent?0:1));
        }
    }
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::removeReportFromDb(const QString &reportUid)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("SELECT ReportFile FROM ReportTable WHERE ReportUid=?");
    query.addBindValue(reportUid);
    query.exec();
    while (query.next()) {
        QSqlRecord rec = query.record();
        QString file = QString("%1/%2").arg(mainWindow->getDbLocation(), rec.value(0).toString());
        QFile(file).remove();
        QDir().rmpath(file.left(file.lastIndexOf('/')));
    }
    query.prepare("DELETE FROM ReportTable WHERE ReportUid=?");
    query.addBindValue(reportUid);
    query.exec();
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::updateImageFile(const QString &imageUid, const QString &imageFile)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("UPDATE ImageTable SET ImageFile=? WHERE ImageUid=?");
    query.addBindValue(imageFile);
    query.addBindValue(imageUid);
    query.exec();
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::updateReportFile(const QString &reportUid, const QString &reportFile)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("UPDATE ReportTable SET ReportFile=? WHERE ReportUid=?");
    query.addBindValue(reportFile);
    query.addBindValue(reportUid);
    query.exec();
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::updateReportStatus(const ReportRecord &report)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("UPDATE ReportTable SET ContentTime=?, IsCompleted=?, IsVerified=?, ReportFile=? WHERE ReportUid=?");
    query.addBindValue(report.contentTime.toString("yyyy-MM-dd hh:mm:ss"));
    query.addBindValue(report.isCompleted?QObject::tr("Yes"):QObject::tr("No"));
    query.addBindValue(report.isVerified?QObject::tr("Yes"):QObject::tr("No"));
    query.addBindValue(report.reportFile);
    query.addBindValue(report.reportUid);
    query.exec();
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}

bool StudyDbManager::updateReportPrintStatus(const QString &reportUid, bool printed)
{
    QSqlDatabase db = QSqlDatabase::database(STUDY_DB_CONNECTION_NAME);
    QSqlQuery query(db);
    query.prepare("UPDATE ReportTable SET IsPrinted=? WHERE ReportUid=?");
    query.addBindValue(printed?QObject::tr("Yes"):QObject::tr("No"));
    query.addBindValue(reportUid);
    query.exec();
    lastError = query.lastError().text();
    return query.lastError().type()==QSqlError::NoError;
}
