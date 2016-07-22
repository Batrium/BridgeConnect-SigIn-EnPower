// -------------------------------------------------------------
//  bridgeconnect firmware for SigIn monitor to transmit to EnPower charger
//  version 1.0  22-July-2016 initial release
// -------------------------------------------------------------
// targets defined for charger based on operating modes

const uint16_t HIPOWER_MAX_VOLT = 1230; // 0.1v/bit  typically 123.0v (30S @ 4.10v)
const uint16_t HIPOWER_MAX_AMP  =  250; // 0.1A/bit  typically  25.0A 
const uint16_t LOPOWER_MAX_VOLT = 1230; // 0.1v/bit  typically 123.0v
const uint16_t LOPOWER_MAX_AMP  =   10; // 0.1A/bit  typically   1.0A

// -------------------------------------------------------------
// variable related to canbus port 2
#include <Metro.h>
#include <SPI.h>
#include "mcp_can.h"

#define TRACE_TX_MSG    (false)
#define TRACE_RX_MSG    (true)
#define TRACE_CALC_TGTS (true)

const int  SPI_CS_PIN = 28;  // pin assignement for CAN2
MCP_CAN    CAN2(SPI_CS_PIN); // Set CAN2 SPI CS pin

unsigned long canRxId;       // RX can identifier
byte          canRxLen;      // RX packet length
byte          canRxBuf[8];   // RX data buffer
byte          canTxBuf[8];   // TX data buffer

Metro canRxTimer = Metro(10);  // check charger status every 10 milliseconds
Metro canTxTimer = Metro(500); // send charger targets every 500 miliseconds

// -------------------------------------------------------------

#include     "SigStateMgmt.h"
#define      TRACE_SIGIN_MODE (true)
#define      PULSE_INPUT_PIN  (25)
SigStateMgmt SigIn;
EnumSigState SigInState;
Metro        sigInTimer = Metro(1000); // refresh sigin monitor every 1000 miliseconds

// -------------------------------------------------------------
// -------------------------------------------------------------
// -------------------------------------------------------------

static uint8_t hex[17] = "0123456789abcdef";

typedef union
{
  uint16_t val;
  uint8_t dat[2];
} UINT16UNION_t;

void hexDump(uint8_t dumpLen, uint8_t *bytePtr)
{
  uint8_t working;
  while ( dumpLen-- ) {
    working = *bytePtr++;
    Serial.write( hex[ working >> 4 ] );
    Serial.write( hex[ working & 15 ] );
  }
  Serial.write('\r');
  Serial.write('\n');
}

// -------------------------------------------------------------

void setup(void)
{
  Serial.begin(115200);
  delay(1000); // wait for serial port ready

  Serial.println("BridgeConnect SigIn-EnPower controller v1.0");

  while (CAN_OK != CAN2.begin(CAN_500KBPS))
  {
    Serial.println("CAN2 init fail");
    delay(100);
  }
  Serial.println("CAN2 init ok!");

  SigIn.setup(PULSE_INPUT_PIN);
  delay(1000);
  Serial.println("SigIn monitor enabled.");

  canRxTimer.reset();
  canTxTimer.reset();
  sigInTimer.reset();
}

// -------------------------------------------------------------

void loop(void)
{
  if ( canRxTimer.check() )
  {
    while (CAN_OK == CAN2.readMsgBufID(&canRxId, &canRxLen, canRxBuf))
    {
      CanbusMgmt_EnPower_FilterReadMsg();
    }
  }

  if ( canTxTimer.check() )
  {
    CanbusMgmt_EnPower_Transmit_Targets();
  }

  SigIn.loop();
  if ( sigInTimer.check() )
  {
    RefreshSigInState();
  }
}

// -------------------------------------------------------------

void CanbusMgmt_EnPower_FilterReadMsg()
{
  switch (canRxId)
  {
    case 0x18EB2440:
      CanbusMgmt_EnPower_Parse_ActualStatus();
      break;
    default:
      // if nothing else matches, do the default
      // default is optional
      // hexDump( sizeof(canRxBuf), (uint8_t *)&canRxBuf );
      break;
  }
}

void CanbusMgmt_EnPower_Parse_ActualStatus()
{
  UINT16UNION_t volt;
  UINT16UNION_t amp;

  volt.dat[0] = canRxBuf[2];
  volt.dat[1] = canRxBuf[3];
  amp.dat[0] = canRxBuf[4];
  amp.dat[1] = canRxBuf[5];

#if TRACE_RX_MSG
  Serial.print(F("RX Actuals ***                    "));
  Serial.print(" volt=");
  Serial.print(volt.val);
  Serial.print(" amp=");
  Serial.print(amp.val);
  Serial.print(" run=");
  Serial.print(canRxBuf[1]);
  Serial.print(" status=");
  Serial.print(canRxBuf[0]);
  Serial.print("\n\r");
  hexDump( sizeof(canRxBuf), (uint8_t *)&canRxBuf );
#endif
}

// -------------------------------------------------------------

void CanbusMgmt_EnPower_Transmit_Targets()
{
  uint16_t MaxVolt = 510;  // 0.1v/bit
  uint16_t MaxAmp =   10;  // 0.1A/bit
  bool ChargeDisable = false;

#if TRACE_CALC_TGTS

  switch (SigInState)
  {
    case SIG_STATE_OFF:
      MaxVolt = 0;
      MaxAmp  = 0;
      ChargeDisable = true;
      break;

    case SIG_STATE_COMPLETED_CHARGE:
      MaxVolt = 0;
      MaxAmp  = 0;
      ChargeDisable = true;
      break;

    case SIG_STATE_LOW_CURRENT:
      MaxVolt = LOPOWER_MAX_VOLT;
      MaxAmp  = LOPOWER_MAX_AMP;
      ChargeDisable = false;
      break;

    case SIG_STATE_HIGH_CURRENT:
      MaxVolt = HIPOWER_MAX_VOLT;
      MaxAmp  = HIPOWER_MAX_AMP;
      ChargeDisable = false;
      break;

    case SIG_STATE_DISABLE:
      MaxVolt = 0;
      MaxAmp  = 0;
      ChargeDisable = true;
      break;

    case SIG_STATE_ENABLE:
      MaxVolt = HIPOWER_MAX_VOLT;
      MaxAmp  = HIPOWER_MAX_AMP;
      ChargeDisable = false;
      break;

    case SIG_STATE_UNDEFINED:
      MaxVolt = 0;
      MaxAmp  = 0;
      ChargeDisable = true;
      break;
  }

#endif

  CanbusMgmt_EnPower_Write_Targets(MaxVolt, MaxAmp, ChargeDisable);

#if TRACE_TX_MSG
  Serial.print(F("TX Targets                    "));
  Serial.print(" MaxVolt=");
  Serial.print(MaxVolt);
  Serial.print(" MaxAmp=");
  Serial.print(MaxAmp);
  Serial.print(" Disable=");
  Serial.print(ChargeDisable);
  Serial.print("\n\r");
  hexDump( sizeof(canTxBuf), (uint8_t *)&canTxBuf );
#endif
}

void CanbusMgmt_EnPower_Write_Targets(uint16_t MaxVolt, uint16_t MaxAmp,  bool ChargeDisable)
{
  // 0.1A/bit 3200 offset (hence -320.0A to +400.0A) hence negative is charging
  uint16_t AmpOffset = 3200 - MaxAmp;

  canTxBuf[0] = (ChargeDisable) ? 0 : 252;
  canTxBuf[1] = lowByte(MaxVolt);
  canTxBuf[2] = highByte(MaxVolt);
  canTxBuf[3] = lowByte(AmpOffset);
  canTxBuf[4] = highByte(AmpOffset);
  canTxBuf[5] = (ChargeDisable) ? 0x03 : 0x05;  // Green LED status on Charger not lite for 0.5s
  canTxBuf[6] = 0xFF;  // Reserved
  canTxBuf[7] = 0xFF;  // Reserved

  // send data: id = 0x01, extended, data len = 8, testData
  CAN2.sendMsgBuf(0x18E54024, 1, 8, canTxBuf);
}

// -------------------------------------------------------------

void RefreshSigInState()
{
  SigInState = SigIn.ReadSigState();

#if TRACE_SIGIN_MODE
  Serial.print("Pulse Freqency (Hz): ");
  Serial.print(SigIn.ReadPulseFreqHz());
  Serial.print("   ");

  switch (SigInState)
  {
    case SIG_STATE_OFF:
      Serial.println("SIG_STATE_OFF");
      break;

    case SIG_STATE_COMPLETED_CHARGE:
      Serial.println("SIG_STATE_COMPLETED_CHARGE");
      break;

    case SIG_STATE_LOW_CURRENT:
      Serial.println("SIG_STATE_LOW_CURRENT");
      break;

    case SIG_STATE_HIGH_CURRENT:
      Serial.println("SIG_STATE_HIGH_CURRENT");
      break;

    case SIG_STATE_DISABLE:
      Serial.println("SIG_STATE_DISABLE");
      break;

    case SIG_STATE_ENABLE:
      Serial.println("SIG_STATE_ENABLE");
      break;

    case SIG_STATE_UNDEFINED:
      Serial.println("SIG_STATE_UNDEFINED");
      break;
  }
#endif
}

// -------------------------------------------------------------


