#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "TempFile.hpp"
#include "storage/BTree.hpp"
#include "storage/FilePageStore.hpp"
#include "storage/FileReservationRepository.hpp"
#include "storage/MemoryPageStore.hpp"
#include "storage/ReservationRecordCodec.hpp"
#include "storage/UserFile.hpp"

namespace tecflix::users {

using testing::TempFile;

namespace {

Reservation reservation(const std::string& id, std::int64_t expiresAt = 5000,
                        ReservationStatus status = ReservationStatus::Active) {
    return Reservation{id, "user-1", 1234, status, 1000, expiresAt};
}

}  // namespace

TEST(ReservationRecordCodecTest, SurvivesEncodeAndDecode) {
    const Reservation original{std::string(kMaxIdLength, 'a'), std::string(kMaxIdLength, 'b'), 99,
                               ReservationStatus::Settled, 1'800'000'000, 1'800'086'400};
    ReservationBuffer buffer{};

    ReservationRecordCodec::encode(original, buffer);
    const Reservation decoded = ReservationRecordCodec::decode(buffer);

    EXPECT_EQ(decoded.id, original.id);
    EXPECT_EQ(decoded.userId, original.userId);
    EXPECT_EQ(decoded.bytes, 99U);
    EXPECT_EQ(decoded.status, ReservationStatus::Settled);
    EXPECT_EQ(decoded.createdAt, original.createdAt);
    EXPECT_EQ(decoded.expiresAt, original.expiresAt);
}

TEST(ReservationRecordCodecTest, RejectsTooLongIdsAndUnknownStatus) {
    ReservationBuffer buffer{};
    Reservation tooLong = reservation("x");
    tooLong.id = std::string(kMaxIdLength + 1, 'x');

    EXPECT_THROW(ReservationRecordCodec::encode(tooLong, buffer), std::invalid_argument);
    EXPECT_THROW(ReservationRecordCodec::decode(ReservationBuffer{}), std::runtime_error);
}

TEST(FileReservationRepositoryTest, AddsAndFindsReservations) {
    const TempFile temp{"reservations_basic.dat"};
    MemoryPageStore pages;
    BTree index{pages};
    FileReservationRepository repository{temp.path(), index};

    EXPECT_TRUE(repository.add(reservation("res-1")));

    ASSERT_TRUE(repository.find("res-1").has_value());
    EXPECT_EQ(repository.find("RES-1")->bytes, 1234U);  // el id no distingue mayúsculas
    EXPECT_FALSE(repository.find("res-2").has_value());
}

TEST(FileReservationRepositoryTest, RejectsDuplicateIds) {
    const TempFile temp{"reservations_duplicate.dat"};
    MemoryPageStore pages;
    BTree index{pages};
    FileReservationRepository repository{temp.path(), index};
    repository.add(reservation("res-1"));

    EXPECT_FALSE(repository.add(reservation("res-1")));
}

TEST(FileReservationRepositoryTest, UpdateReplacesTheStoredReservation) {
    const TempFile temp{"reservations_update.dat"};
    MemoryPageStore pages;
    BTree index{pages};
    FileReservationRepository repository{temp.path(), index};
    repository.add(reservation("res-1"));
    Reservation changed = reservation("res-1");
    changed.status = ReservationStatus::Settled;
    changed.bytes = 77;

    repository.update(changed);

    EXPECT_EQ(repository.find("res-1")->status, ReservationStatus::Settled);
    EXPECT_EQ(repository.find("res-1")->bytes, 77U);
    EXPECT_THROW(repository.update(reservation("res-9")), std::logic_error);
}

TEST(FileReservationRepositoryTest, FindsOnlyActiveReservationsThatAlreadyExpired) {
    const TempFile temp{"reservations_expired.dat"};
    MemoryPageStore pages;
    BTree index{pages};
    FileReservationRepository repository{temp.path(), index};
    repository.add(reservation("due", 2000));
    repository.add(reservation("later", 9000));
    repository.add(reservation("settled", 2000, ReservationStatus::Settled));
    repository.add(reservation("released", 2000, ReservationStatus::Released));

    const auto expired = repository.findActiveExpiredBy(3000);

    ASSERT_EQ(expired.size(), 1U);
    EXPECT_EQ(expired.front().id, "due");
    EXPECT_EQ(repository.findActiveExpiredBy(2000).size(), 1U);  // vence justo en ese segundo
    EXPECT_TRUE(repository.findActiveExpiredBy(1999).empty());
}

TEST(FileReservationRepositoryTest, DataSurvivesRestart) {
    const TempFile dataFile{"reservations_persist.dat"};
    const TempFile indexFile{"reservations_persist.idx"};
    {
        FilePageStore pages{indexFile.path()};
        BTree index{pages};
        FileReservationRepository repository{dataFile.path(), index};
        repository.add(reservation("res-1"));
        repository.add(reservation("res-2"));
        Reservation changed = reservation("res-2");
        changed.status = ReservationStatus::Released;
        repository.update(changed);
    }

    FilePageStore pages{indexFile.path()};
    BTree index{pages};
    const FileReservationRepository repository{dataFile.path(), index};

    EXPECT_EQ(repository.find("res-1")->status, ReservationStatus::Active);
    EXPECT_EQ(repository.find("res-2")->status, ReservationStatus::Released);
}

TEST(FixedRecordFileTest, RefusesAFileThatBelongsToAnotherKindOfRecord) {
    const TempFile temp{"fixed_mismatch.dat"};
    {
        UserFile users{temp.path()};  // la firma es la de users.dat
    }
    MemoryPageStore pages;
    BTree index{pages};

    EXPECT_THROW((FileReservationRepository{temp.path(), index}), std::runtime_error);
}

}  // namespace tecflix::users
