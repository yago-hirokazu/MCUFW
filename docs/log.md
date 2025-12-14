# ログ

## 2024-04-21

### delay

```c
__weak void dlyinit(void);
void msec(DELAY_UWORD ms);
__weak void usec(DELAY_UWORD us)
```

dlyinitとusecの実装  
```
MCUFW\elib\stm32\delay_adapt.c
MCUFW\elib\windows
MCUFW\elib\cygwin
```

cygwinのシリアル端末設定はどこにある？HDD？gridなんかで検索すれば見つかるかも。  

### ttrace

Time TRACE

下記全部環境依存
```c
void ttrinit(void);
void ttrterm(void);
void ttrstart(void);
uint32_t ttrstop(void);
void ttraceint(void);
```

### ctrace

Clock TRACE  
システムクロック周波数でタイマカウントをカウントしてクロック数で処理速度を測るためのモジュール。  
DWT（Debug Watch and Trace）を使えばもっと簡単では。TBC  
Windowsの場合は？  

* Windowsコンピュータを使ったサブミリ秒計時手法の比較  
  [https://hsuh.repo.nii.ac.jp/record/6845/files/KJ00005435853.pdf](https://hsuh.repo.nii.ac.jp/record/6845/files/KJ00005435853.pdf)

  + QueryPerformanceCounter
  + RDTSC命令  
    [http://nullull.blog100.fc2.com/blog-entry-15.html](http://nullull.blog100.fc2.com/blog-entry-15.html)
  + DirectMusic: IReferenceClock::GetTime()  
    [https://learn.microsoft.com/ja-jp/windows/win32/api/strmif/nf-strmif-ireferenceclock-gettime](https://learn.microsoft.com/ja-jp/windows/win32/api/strmif/nf-strmif-ireferenceclock-gettime)

Cygwinの場合は？  


### ucmd.hの場所

Cmd/に移動？TBD。Adaptationが必要な要素


