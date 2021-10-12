/*
 * 만든이 : 남지원
 * 날짜   : 2021.10.12
 * 내용   : 크래비스 이미지를 받아서 편광분리한다음 OpenCV로 각도별 이미지 출력하는 예제
*/

#include <stdio.h>
#include <tchar.h>
#include <conio.h>
#include <windows.h>
#include "VirtualFG40.h"
#include "CCriticalSection.h"

#pragma comment (lib, "VirtualFG40.lib")

#include <opencv2/opencv.hpp>

// 원본 이미지 포인터
__int32 g_Buffersize;
BYTE* g_pImage = NULL;

// 분할 이미지 포인터
__int32 g_DegreeBufferSize;
BYTE* g_pDegree0 = NULL;
BYTE* g_pDegree45 = NULL;
BYTE* g_pDegree90 = NULL;
BYTE* g_pDegree135 = NULL;

// 원본 이미지 사이즈
__int32 g_width = 0, g_height = 0;

// 분할 이미지 사이즈
__int32 g_split_width = 0, g_split_height = 0;

// 동기화를 위한 이벤트 핸들러
HANDLE g_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

// 크리티컬 섹션을 위한 객체
CCriticalSection g_cs = CCriticalSection();

/// <summary>
/// Grab 콜백 시 메소드
/// </summary>
/// <param name="event"></param>
/// <param name="pImage"></param>
/// <param name="pUserDefine"></param>
/// <returns></returns>
__int32 GrabFunction(__int32 event, void* pImage, void* pUserDefine)
{
	if (event != EVENT_NEW_IMAGE)
		return -1;

	{
		// 크리티컬 섹션 
		CAutoLock(*g_cs);
		memcpy((void*)g_pImage, pImage, g_Buffersize);
	}

	// 동기화를 위한 이벤트
	SetEvent(g_hEvent);

	return 0;
}

void SetFeature(int hDevice)
{
	__int32 status = MCAM_ERR_SUCCESS;

	// Set Trigger Mode : OFF -> Continuous
	status = ST_SetEnumReg(hDevice, MCAM_TRIGGER_MODE, TRIGGER_MODE_OFF);
	if (status != MCAM_ERR_SUCCESS)
	{
		printf("Write Register failed : %d\n", status);
	}

	// Continuous모드를 사용하기 위한 세팅
	status = ST_SetContinuousGrabbing(hDevice, MCAMU_CONTINUOUS_GRABBING_ENABLE);
	if (status != MCAM_ERR_SUCCESS)
	{
		printf("Write Register failed : %d\n", status);
	}

	// Set PixelFormat : BayerRG8
	status = ST_SetEnumReg(hDevice, MCAM_PIXEL_FORMAT, PIXEL_FORMAT_MONO8);
	if (status != MCAM_ERR_SUCCESS)
	{
		printf("Write Register failed : %d\n", status);
	}

	// Set GrabTimeout to 5 sec
	status = ST_SetGrabTimeout(hDevice, 5000);
	if (status != MCAM_ERR_SUCCESS)
	{
		printf("Write Register failed : %d\n", status);
	}

	// FilterDriver : Must be "OFF" when debugging. Otherwise, the BSOD may occur.
	status = ST_SetEnumReg(hDevice, MCAM_DEVICE_FILTER_DRIVER_MODE, DEVICE_FILTER_DRIVER_MODE_OFF);
	if (status != MCAM_ERR_SUCCESS)
	{
		printf("Write Register failed : %d\n", status);
	}

	//Heartbeat Disable : Must be "true" when debugging. Otherwise, the device will be disconnected.
	status = ST_SetBoolReg(hDevice, GEV_GVCP_HEARTBEAT_DISABLE, false);
	if (status != MCAM_ERR_SUCCESS)
	{
		printf("Write Register failed : %d\n", status);
	}
}

/// <summary>
/// 한장의 이미지를 각도별 분리
/// </summary>
/// <param name="pSrcImg">크래비스 이미지 포인터</param>
/// <param name="ImageWidth">원래 이미지의 width</param>
/// <param name="ImageHeight">원래 이미지의 height</param>
void SplitImages()
{
	int i, j;
	int idx1, idx2;
	int cnt = 0;

	for (j = 0; j < g_height; j += 2)
	{
		idx1 = g_width * j;
		idx2 = idx1 + g_width;

		for (i = 0; i < g_width; i += 2)
		{
			g_pDegree0[cnt] = g_pImage[idx1 + i];
			g_pDegree45[cnt] = g_pImage[idx1 + i + 1];
			g_pDegree90[cnt] = g_pImage[idx2+ i];
			g_pDegree135[cnt] = g_pImage[idx2 + i + 1];
			cnt++;
		}
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	__int32 status = MCAM_ERR_SUCCESS;
	__int32 hDevice;
	unsigned __int32 CamNum;

	// system open
	ST_InitSystem();

	// Update Device List
	status = ST_UpdateDevice();
	if (status != MCAM_ERR_SUCCESS)
	{
		printf("Update device list failed : %d\n", status);
		ST_FreeSystem();
		return 0;
	}

	ST_GetAvailableCameraNum(&CamNum);
	if (CamNum <= 0)
	{
		printf("No device!\n");
		ST_FreeSystem();
		return 0;
	}

	// camera open
	status = ST_OpenDevice(0, &hDevice);
	if (status != MCAM_ERR_SUCCESS)
	{
		printf("Open device failed : %d\n", status);
		ST_FreeSystem();
		return 0;
	}

	ST_SetCallbackFunction(hDevice, EVENT_NEW_IMAGE, GrabFunction, NULL);


	SetFeature(hDevice);

	try
	{
		//Get Width
		status = ST_GetIntReg(hDevice, MCAM_WIDTH, &g_width);
		if (status != MCAM_ERR_SUCCESS)
		{
			printf("Read Register failed : ");
			throw status;
		}

		//Get Height
		status = ST_GetIntReg(hDevice, MCAM_HEIGHT, &g_height);
		if (status != MCAM_ERR_SUCCESS)
		{
			printf("Read Register failed : ");
			throw status;
		}

		// 이미지 메모리 할당 및 초기화
		g_Buffersize = g_width * g_height;
		g_pImage = (BYTE*)malloc(g_Buffersize);
		memset((void*)g_pImage, 0, g_Buffersize);

		// 분할 이미지 사이즈 = 원본 이미지 사이즈 / 2
		g_split_width = g_width / 2;
		g_split_height = g_height / 2;
		g_DegreeBufferSize = g_split_width * g_split_height;

		// 분할 이미지 메모리 할당 및 초기화
		g_pDegree0 = (BYTE*)malloc(g_DegreeBufferSize);
		g_pDegree45 = (BYTE*)malloc(g_DegreeBufferSize);
		g_pDegree90 = (BYTE*)malloc(g_DegreeBufferSize);
		g_pDegree135 = (BYTE*)malloc(g_DegreeBufferSize);

		memset((void*)g_pDegree0, 0, g_DegreeBufferSize);
		memset((void*)g_pDegree45, 0, g_DegreeBufferSize);
		memset((void*)g_pDegree90, 0, g_DegreeBufferSize);
		memset((void*)g_pDegree135, 0, g_DegreeBufferSize);


		printf("Software trigger mode\n");
		printf("-----------------------------\n\n");
		printf("Press <Enter> to start.\n");
		_getch();

		// Acquistion Start
		status = ST_AcqStart(hDevice);
		if (status != MCAM_ERR_SUCCESS)
		{
			printf("Acquisition start failed : ");
			throw status;
		}

		ST_GrabStartAsync(hDevice, -1);

		cv::Mat cv_img, cv_img_0, cv_img_45, cv_img_90, cv_img_135;

		// 크래비스 이미지 -> OpenCV::Mat
		// 원래 이미지
		cv_img = cv::Mat(
			g_height,			// row : 열의 수(height)
			g_width,			// col : 행의 수(width)
			CV_8UC1,			// type : 이미지 타입
			g_pImage,			// *data : 이미지 데이터
			g_width				// step : 한 행의 원소 수
		);

		// 0도 이미지
		cv_img_0 = cv::Mat(
			g_split_height,		
			g_split_width,		
			CV_8UC1,			
			g_pDegree0,			
			g_split_width		
		);

		// 45도 이미지
		cv_img_45 = cv::Mat(
			g_split_height,		
			g_split_width,		
			CV_8UC1,			
			g_pDegree45,		
			g_split_width		
		);

		// 90도 이미지
		cv_img_90 = cv::Mat(
			g_split_height,	
			g_split_width,	
			CV_8UC1,		
			g_pDegree90,	
			g_split_width	
		);

		// 135도 이미지
		cv_img_135 = cv::Mat(
			g_split_height,	
			g_split_width,	
			CV_8UC1,		
			g_pDegree135,	
			g_split_width	
		);

		while (1)
		{
			if (WaitForSingleObject(g_hEvent, 3000) == WAIT_TIMEOUT)
			{
				printf("타임아웃");
			}
			else
			{
				{
					// 크리티컬 섹션 
					CAutoLock(*g_cs);
					
					// 한장의 이미지를 각도별 분리
					SplitImages();
				}

				// 이미지 출력
				cv::imshow("0", cv_img_0);
				cv::imshow("45", cv_img_45);
				cv::imshow("90", cv_img_90);
				cv::imshow("135", cv_img_135);
				
				// 이미지 재 출력 : 10ms 마다
				cv::waitKey(10);

			}

			// 키보드 입력 대기
			if (_kbhit())
			{
				break;
			}
		}

		// Acquistion Stop
		status = ST_AcqStop(hDevice);
		if (status != MCAM_ERR_SUCCESS)
		{
			printf("Acquisition stop failed : ");
			throw status;
		}
	}
	catch (__int32 err)
	{
		printf("%d\n", err);
	}

	// 할당 된 메모리 해제
	free(g_pImage);
	free(g_pDegree0);
	free(g_pDegree45);
	free(g_pDegree90);
	free(g_pDegree135);

	// Close Device
	ST_CloseDevice(hDevice);

	// Free system
	ST_FreeSystem();

	return 0;
}

