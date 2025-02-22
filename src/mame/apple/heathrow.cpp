// license:BSD-3-Clause
// copyright-holders:R. Belmont
/*
    Apple "Grand Central", "O'Hare", "Heathrow" and "Paddington" PCI ASICs
    Emulation by R. Belmont

    These ASICs sit on the PCI bus and provide what came to be known as "Mac I/O",
    including:
    - A VIA to interface with Cuda / an enhanced VIA that can speak SPI to interface with PG&E (O'Hare and later)
    - Serial
    - SWIM3 floppy
    - MESH SCSI ("Macintosh Enhanced SCSI Handler"), a 5394/96 clone with some features Apple didn't use removed)
    - ATA
    - Ethernet (10 Mbps for Heathrow, 10/100 for Paddington)
    - Audio
    - Descriptor-based DMA engine, as described in "Macintosh Technology in the Common Hardware Reference Platform"
*/

#include "emu.h"
#include "heathrow.h"

#include "bus/rs232/rs232.h"
#include "formats/ap_dsk35.h"

#define LOG_GENERAL (1U << 0)
#define LOG_IRQ     (1U << 1)

#define VERBOSE (0)
#include "logmacro.h"

static constexpr u32 C7M  = 7833600;
static constexpr u32 C15M = (C7M * 2);

//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

DEFINE_DEVICE_TYPE(HEATHROW, heathrow_device, "heathrow", "Apple Heathrow PCI I/O ASIC")
DEFINE_DEVICE_TYPE(PADDINGTON, paddington_device, "paddington", "Apple Paddington PCI I/O ASIC")
DEFINE_DEVICE_TYPE(OHARE, ohare_device, "ohare", "Apple O'Hare PCI I/O ASIC")
DEFINE_DEVICE_TYPE(GRAND_CENTRAL, grandcentral_device, "grndctrl", "Apple Grand Central PCI I/O ASIC")

//-------------------------------------------------
//  ADDRESS_MAP
//-------------------------------------------------
/*          Grand Central           O'Hare                  Heathrow/Paddington
0x10000     SCSI0                   SCSI0                   SCSI
0x11000     MACE Ethernet           (unused)                "BigMac" Ethernet
0x12000     SCC "compatibility"     SCC compat              SCC compat
0x13000     SCC "MacRisc"           SCC MacRisc             SCC MacRisc
0x14000     Audio                   Audio                   Audio
0x15000     SWIM3                   SWIM3                   SWIM3
0x16000     VIA                     VIA                     VIA
0x17000     VIA                     VIA                     VIA
0x18000     SCSI1                   (unused)                (unused)
0x19000     Ethernet MAC PROM       (unused)                ADB Master Cell
0x1a000     (external IOBus)        (external IOBus)        (external IOBus)
0x1b000     (external IOBus)        (external IOBus)        (external IOBus)
0x1c000     (external IOBus)        (external IOBus)        (external IOBus)
0x1d000     (external IOBus)        (external IOBus)        (external IOBus)
0x1e000     (external IOBus)        (unused)
0x1f000     (external IOBus)        (unused)
0x20000     (external IOBus)        ATA bus 0               ATA bus 0
0x21000     (external IOBus)        ATA bus 1               ATA bus 1
0x60000     (128K BAR, no)          PRAM                    PRAM
0x70000     (128K BAR, no)          PRAM                    PRAM
*/
void grandcentral_device::map(address_map &map)
{
	map(0x00000, 0x00fff).rw(FUNC(grandcentral_device::macio_r), FUNC(grandcentral_device::macio_w));
	map(0x08800, 0x0881f).m(m_dma_audio_out, FUNC(dbdma_device::map));
	map(0x08900, 0x0891f).m(m_dma_audio_in, FUNC(dbdma_device::map));
	map(0x12000, 0x12fff).rw(FUNC(grandcentral_device::scc_r), FUNC(grandcentral_device::scc_w));
	map(0x13000, 0x13fff).rw(FUNC(grandcentral_device::scc_macrisc_r), FUNC(grandcentral_device::scc_macrisc_w));
	map(0x14000, 0x140ff).rw(FUNC(heathrow_device::codec_r), FUNC(heathrow_device::codec_w));
	map(0x15000, 0x15fff).rw(FUNC(grandcentral_device::fdc_r), FUNC(grandcentral_device::fdc_w));
	map(0x16000, 0x17fff).rw(FUNC(grandcentral_device::mac_via_r), FUNC(grandcentral_device::mac_via_w));
}

void heathrow_device::map(address_map &map)
{
	map(0x00000, 0x00fff).rw(FUNC(heathrow_device::macio_r), FUNC(heathrow_device::macio_w));
	map(0x08800, 0x0881f).m(m_dma_audio_out, FUNC(dbdma_device::map));
	map(0x08900, 0x0891f).m(m_dma_audio_in, FUNC(dbdma_device::map));
	map(0x12000, 0x12fff).rw(FUNC(heathrow_device::scc_r), FUNC(heathrow_device::scc_w));
	map(0x13000, 0x13fff).rw(FUNC(heathrow_device::scc_macrisc_r), FUNC(heathrow_device::scc_macrisc_w));
	map(0x14000, 0x140ff).rw(FUNC(heathrow_device::codec_r), FUNC(heathrow_device::codec_w));
	map(0x15000, 0x15fff).rw(FUNC(heathrow_device::fdc_r), FUNC(heathrow_device::fdc_w));
	map(0x16000, 0x17fff).rw(FUNC(heathrow_device::mac_via_r), FUNC(heathrow_device::mac_via_w));
	map(0x60000, 0x7ffff).rw(FUNC(heathrow_device::nvram_r), FUNC(heathrow_device::nvram_w)).umask32(0x000000ff);
}

//-------------------------------------------------
//  device_add_mconfig - add device configuration
//-------------------------------------------------
void heathrow_device::device_add_mconfig(machine_config &config)
{
	R65NC22(config, m_via1, C7M / 10);
	m_via1->readpa_handler().set(FUNC(heathrow_device::via_in_a));
	m_via1->readpb_handler().set(FUNC(heathrow_device::via_in_b));
	m_via1->writepa_handler().set(FUNC(heathrow_device::via_out_a));
	m_via1->writepb_handler().set(FUNC(heathrow_device::via_out_b));
	m_via1->cb2_handler().set(FUNC(heathrow_device::via_out_cb2));
	m_via1->irq_handler().set(FUNC(heathrow_device::set_irq_line<18>));

	DBDMA_CHANNEL(config, m_dma_audio_out, 0);
	m_dma_audio_out->irq_callback().set(FUNC(heathrow_device::set_irq_line<8>));

	DBDMA_CHANNEL(config, m_dma_audio_in, 0);
	m_dma_audio_in->irq_callback().set(FUNC(heathrow_device::set_irq_line<9>));

	SWIM3(config, m_fdc, C15M);
	m_fdc->devsel_cb().set(FUNC(heathrow_device::devsel_w));
	m_fdc->phases_cb().set(FUNC(heathrow_device::phases_w));

	applefdintf_device::add_35_hd(config, m_floppy[0]);
	applefdintf_device::add_35_nc(config, m_floppy[1]);

	SCC85C30(config, m_scc, C7M);
	m_scc->configure_channels(3'686'400, 3'686'400, 3'686'400, 3'686'400);
	m_scc->out_txda_callback().set("printer", FUNC(rs232_port_device::write_txd));
	m_scc->out_txdb_callback().set("modem", FUNC(rs232_port_device::write_txd));

	rs232_port_device &rs232a(RS232_PORT(config, "printer", default_rs232_devices, nullptr));
	rs232a.rxd_handler().set(m_scc, FUNC(z80scc_device::rxa_w));
	rs232a.dcd_handler().set(m_scc, FUNC(z80scc_device::dcda_w));
	rs232a.cts_handler().set(m_scc, FUNC(z80scc_device::ctsa_w));

	rs232_port_device &rs232b(RS232_PORT(config, "modem", default_rs232_devices, nullptr));
	rs232b.rxd_handler().set(m_scc, FUNC(z80scc_device::rxb_w));
	rs232b.dcd_handler().set(m_scc, FUNC(z80scc_device::dcdb_w));
	rs232b.cts_handler().set(m_scc, FUNC(z80scc_device::ctsb_w));
}

void heathrow_device::config_map(address_map &map)
{
	pci_device::config_map(map);
}

//-------------------------------------------------
//  heathrow_device - constructor
//-------------------------------------------------

heathrow_device::heathrow_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, u32 clock)
	: pci_device(mconfig, type, tag, owner, clock),
	  write_irq(*this),
	  write_pb4(*this),
	  write_pb5(*this),
	  write_cb2(*this),
	  read_pb3(*this),
	  read_codec(*this),
	  write_codec(*this),
	  m_maincpu(*this, finder_base::DUMMY_TAG),
	  m_via1(*this, "via1"),
	  m_fdc(*this, "fdc"),
	  m_floppy(*this, "fdc:%d", 0U),
	  m_scc(*this, "scc"),
	  m_dma_audio_in(*this, "dma_audin"),
	  m_dma_audio_out(*this, "dma_audout"),
	  m_pci_memory(*this, ":pci:00.0", AS_DATA),
	  m_cur_floppy(nullptr),
	  m_hdsel(0)
{
	m_toggle = 0;
}

heathrow_device::heathrow_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: heathrow_device(mconfig, HEATHROW, tag, owner, clock)
{
}

paddington_device::paddington_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: heathrow_device(mconfig, PADDINGTON, tag, owner, clock)
{
}

grandcentral_device::grandcentral_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: heathrow_device(mconfig, GRAND_CENTRAL, tag, owner, clock)
{
}

ohare_device::ohare_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: heathrow_device(mconfig, OHARE, tag, owner, clock)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void heathrow_device::common_init()
{
	write_irq.resolve_safe();
	write_pb4.resolve_safe();
	write_pb5.resolve_safe();
	write_cb2.resolve_safe();
	read_pb3.resolve_safe(0);
	read_codec.resolve_safe(0);
	write_codec.resolve_safe();

	m_dma_audio_out->set_address_space(m_pci_memory);

	command = 2; // enable our memory range
	revision = 1;

	m_InterruptEvents = m_InterruptMask = m_InterruptLevels = 0;
	m_InterruptEvents2 = m_InterruptMask2 = m_InterruptLevels2 = 0;
	recalc_irqs();

	save_item(NAME(m_hdsel));
	save_item(NAME(m_InterruptEvents));
	save_item(NAME(m_InterruptMask));
	save_item(NAME(m_InterruptLevels));
	save_item(NAME(m_InterruptEvents2));
	save_item(NAME(m_InterruptMask2));
	save_item(NAME(m_InterruptLevels2));
	save_item(NAME(m_nvram));
}

void heathrow_device::device_start()
{
	common_init();
	add_map(0x80000, M_MEM, FUNC(heathrow_device::map));
	set_ids(0x106b0010, 0x01, 0xff000001, 0x000000);
}

void paddington_device::device_start()
{
	common_init();
	add_map(0x80000, M_MEM, FUNC(heathrow_device::map));
	set_ids(0x106b0017, 0x01, 0xff000001, 0x000000);
}

void grandcentral_device::device_start()
{
	common_init();
	add_map(0x20000, M_MEM, FUNC(heathrow_device::map));    // Grand Central only has 128K of BAR space, the others have 512K
	set_ids(0x106b0002, 0x01, 0xff000001, 0x000000);
}

void ohare_device::device_start()
{
	common_init();
	add_map(0x80000, M_MEM, FUNC(heathrow_device::map));
	set_ids(0x106b0007, 0x01, 0xff000001, 0x000000);
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void heathrow_device::device_reset()
{
	m_hdsel = 0;
}

u8 heathrow_device::via_in_a()
{
	return 0x80;
}

u8 heathrow_device::via_in_b()
{
	return read_pb3() << 3;
}

WRITE_LINE_MEMBER(heathrow_device::via_out_cb2)
{
	write_cb2(state & 1);
}

void heathrow_device::via_out_a(u8 data)
{
	int hdsel = BIT(data, 5);
	if (hdsel != m_hdsel)
	{
		if (m_cur_floppy)
		{
			m_cur_floppy->ss_w(hdsel);
		}
	}
	m_hdsel = hdsel;
}

void heathrow_device::via_out_b(u8 data)
{
	write_pb4(BIT(data, 4));
	write_pb5(BIT(data, 5));
}

WRITE_LINE_MEMBER(heathrow_device::cb1_w)
{
	m_via1->write_cb1(state);
}

WRITE_LINE_MEMBER(heathrow_device::cb2_w)
{
	m_via1->write_cb2(state);
}

u16 heathrow_device::mac_via_r(offs_t offset)
{
	u16 data;

	offset >>= 8;
	offset &= 0x0f;

	if (!machine().side_effects_disabled())
		via_sync();

	data = m_via1->read(offset);

	return (data & 0xff) | (data << 8);
}

void heathrow_device::mac_via_w(offs_t offset, u16 data, u16 mem_mask)
{
	offset >>= 8;
	offset &= 0x0f;

	via_sync();

	if (ACCESSING_BITS_0_7)
		m_via1->write(offset, data & 0xff);
	if (ACCESSING_BITS_8_15)
		m_via1->write(offset, (data >> 8) & 0xff);
}

void heathrow_device::via_sync()
{
	// The via runs at 783.36KHz while the main cpu runs at 15MHz or
	// more, so we need to sync the access with the via clock.  Plus
	// the whole access takes half a (via) cycle and ends when synced
	// with the main cpu again.

	// Get the main cpu time
	u64 cycle = m_maincpu->total_cycles();

	// Get the number of the cycle the via is in at that time
	u64 via_cycle = cycle * m_via1->clock() / m_maincpu->clock();

	// The access is going to start at via_cycle+1 and end at
	// via_cycle+1.5, compute what that means in maincpu cycles (the
	// +1 rounds up, since the clocks are too different to ever be
	// synced).
	u64 main_cycle = (via_cycle * 2 + 3) * m_maincpu->clock() / (2 * m_via1->clock()) + 1;

	// Finally adjust the main cpu icount as needed.
	m_maincpu->adjust_icount(-int(main_cycle - cycle));
}

u16 heathrow_device::swim_r(offs_t offset, u16 mem_mask)
{
	if (!machine().side_effects_disabled())
	{
		m_maincpu->adjust_icount(-5);
	}

	u16 result = m_fdc->read((offset >> 8) & 0xf);
	return result << 8;
}
void heathrow_device::swim_w(offs_t offset, u16 data, u16 mem_mask)
{
	if (ACCESSING_BITS_0_7)
		m_fdc->write((offset >> 8) & 0xf, data & 0xff);
	else
		m_fdc->write((offset >> 8) & 0xf, data >> 8);
}

void heathrow_device::phases_w(u8 phases)
{
	if (m_cur_floppy)
		m_cur_floppy->seek_phase_w(phases);
}

void heathrow_device::devsel_w(u8 devsel)
{
	if (devsel == 1)
		m_cur_floppy = m_floppy[0]->get_device();
	else if (devsel == 2)
		m_cur_floppy = m_floppy[1]->get_device();
	else
		m_cur_floppy = nullptr;

	m_fdc->set_floppy(m_cur_floppy);
	if (m_cur_floppy)
		m_cur_floppy->ss_w(m_hdsel);
}

u32 heathrow_device::macio_r(offs_t offset)
{
	// InterruptLevels = live status of all interrupt lines
	// InterruptMask = mask to determine which bits of InterruptLevels matter
	// InterruptEvents = interrupts allowed to fire by InterruptMask
//  printf("macio_r: offset %x (%x)\n", offset, offset*4);
	switch (offset << 2)
	{
		case 0x10:
			return swapendian_int32(m_InterruptEvents2 & m_InterruptMask2);
		case 0x14:
			return swapendian_int32(m_InterruptMask2);
		case 0x1c:
			return swapendian_int32(m_InterruptLevels2);
		case 0x20:
			return swapendian_int32(m_InterruptEvents & m_InterruptMask);
		case 0x24:
			return swapendian_int32(m_InterruptMask);
		case 0x2c:
			return swapendian_int32(m_InterruptLevels);
	}
	return 0;
}

void heathrow_device::macio_w(offs_t offset, u32 data, u32 mem_mask)
{
	data = swapendian_int32(data);
	mem_mask = swapendian_int32(mem_mask);
//  printf("macio_w: offset %x (%x) data %08x mask %08x\n", offset, offset*4, data, mem_mask);
	switch (offset << 2)
	{
		case 0x14:
			m_InterruptMask2 = data;
			recalc_irqs();
			break;
		case 0x18:  // InterruptClear
			// which interrupt mode?
			if (BIT(m_InterruptMask2, 31))
			{
				// in Mode 1, "1" to bit 31 clears all active interrupts
				if (BIT(data, 31))
				{
					m_InterruptEvents2 = 0;
				}
				else
				{
					m_InterruptEvents2 &= (data ^ 0xffffffff);
				}
			}
			else
			{
				m_InterruptEvents2 &= (data ^ 0xffffffff);
			}
			recalc_irqs();
			break;
		case 0x24:
			m_InterruptMask = data;
			recalc_irqs();
			break;
		case 0x28:  // InterruptClear
			// which interrupt mode?
			if (BIT(m_InterruptMask, 31))
			{
				// in Mode 1, "1" to bit 31 clears all active interrupts
				if (BIT(data, 31))
				{
					m_InterruptEvents = 0;
				}
				else
				{
					m_InterruptEvents &= (data ^ 0xffffffff);
				}
			}
			else
			{
				m_InterruptEvents &= (data ^ 0xffffffff);
			}
			recalc_irqs();
			break;
	}
}

void heathrow_device::recalc_irqs()
{
	LOGMASKED(LOG_IRQ, "%s recalc_irqs: events %08x levels %08x mask %08x\n", tag(), m_InterruptEvents, m_InterruptLevels, m_InterruptMask);
	m_InterruptEvents = m_InterruptLevels & m_InterruptMask;
	m_InterruptEvents2 = m_InterruptLevels2 & m_InterruptMask2;
	if (m_InterruptEvents || m_InterruptEvents2)
	{
		write_irq(ASSERT_LINE);
	}
	else
	{
		write_irq(CLEAR_LINE);
	}
}

template<int bit> WRITE_LINE_MEMBER(heathrow_device::set_irq_line)
{
	if (bit < 32)
	{
		if (state == ASSERT_LINE)
		{
			m_InterruptLevels |= (1 << bit);
		}
		else
		{
			m_InterruptLevels &= ((1 << bit) ^ 0xffffffff);
		}
	}
	else
	{
		if (state == ASSERT_LINE)
		{
			m_InterruptLevels2 |= (1 << (bit-32));
		}
		else
		{
			m_InterruptLevels2 &= ((1 << (bit-32)) ^ 0xffffffff);
		}
	}
	recalc_irqs();
}

u8 heathrow_device::fdc_r(offs_t offset)
{
	return m_fdc->read(offset >> 9);
}

void heathrow_device::fdc_w(offs_t offset, u8 data)
{
	m_fdc->write(offset >> 9, data);
}

u8 heathrow_device::nvram_r(offs_t offset)
{
	return m_nvram[offset>>2];
}

void heathrow_device::nvram_w(offs_t offset, u8 data)
{
	m_nvram[offset>>2] = data;
}

u16 heathrow_device::scc_r(offs_t offset)
{
	u16 result = m_scc->dc_ab_r(offset);
	return (result << 8) | result;
}

void heathrow_device::scc_w(offs_t offset, u16 data)
{
	m_scc->dc_ab_w(offset, data >> 8);
}

u8 heathrow_device::scc_macrisc_r(offs_t offset)
{
	switch ((offset >> 4) & 0xf)
	{
		case 0:
			return m_scc->cb_r(0);

		case 1:
			return m_scc->db_r(0);

		case 2:
			return m_scc->ca_r(0);

		case 3:
			return m_scc->da_r(0);
	}
	return 0;
}

void heathrow_device::scc_macrisc_w(offs_t offset, u8 data)
{
	switch ((offset >> 4) & 0xf)
	{
		case 0:
			return m_scc->cb_w(0, data);

		case 1:
			return m_scc->db_w(0, data);

		case 2:
			return m_scc->ca_w(0, data);

		case 3:
			return m_scc->da_w(0, data);
	}
}

// Audio support
uint32_t heathrow_device::codec_r(offs_t offset)
{
	return read_codec(offset);
}

void heathrow_device::codec_w(offs_t offset, uint32_t data)
{
	write_codec(offset, data);
}

u32 heathrow_device::codec_dma_read(u32 offset)
{
	return m_dma_audio_out->dma_read(offset);
}

void heathrow_device::codec_dma_write(u32 offset, u32 data)
{
	m_dma_audio_in->dma_write(offset, data);
}
