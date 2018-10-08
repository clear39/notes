void Heap::PreZygoteFork() {
  if (!HasZygoteSpace()) {
    // We still want to GC in case there is some unreachable non moving objects that could cause a
    // suboptimal bin packing when we compact the zygote space.
    CollectGarbageInternal(collector::kGcTypeFull, kGcCauseBackground, false);
    // Trim the pages at the end of the non moving space. Trim while not holding zygote lock since
    // the trim process may require locking the mutator lock.
    non_moving_space_->Trim();
  }
  Thread* self = Thread::Current();
  MutexLock mu(self, zygote_creation_lock_);
  // Try to see if we have any Zygote spaces.
  if (HasZygoteSpace()) {
    return;
  }
  Runtime::Current()->GetInternTable()->AddNewTable();
  Runtime::Current()->GetClassLinker()->MoveClassTableToPreZygote();
  VLOG(heap) << "Starting PreZygoteFork";
  // The end of the non-moving space may be protected, unprotect it so that we can copy the zygote
  // there.
  non_moving_space_->GetMemMap()->Protect(PROT_READ | PROT_WRITE);
  const bool same_space = non_moving_space_ == main_space_;
  if (kCompactZygote) {
    // Temporarily disable rosalloc verification because the zygote
    // compaction will mess up the rosalloc internal metadata.
    ScopedDisableRosAllocVerification disable_rosalloc_verif(this);
    ZygoteCompactingCollector zygote_collector(this, is_running_on_memory_tool_);
    zygote_collector.BuildBins(non_moving_space_);
    // Create a new bump pointer space which we will compact into.
    space::BumpPointerSpace target_space("zygote bump space", non_moving_space_->End(),non_moving_space_->Limit());
    // Compact the bump pointer space to a new zygote bump pointer space.
    bool reset_main_space = false;
    if (IsMovingGc(collector_type_)) {
      if (collector_type_ == kCollectorTypeCC) {
        zygote_collector.SetFromSpace(region_space_);
      } else {
        zygote_collector.SetFromSpace(bump_pointer_space_);
      }
    } else {
      CHECK(main_space_ != nullptr);
      CHECK_NE(main_space_, non_moving_space_)  << "Does not make sense to compact within the same space";
      // Copy from the main space.
      zygote_collector.SetFromSpace(main_space_);
      reset_main_space = true;
    }
    zygote_collector.SetToSpace(&target_space);
    zygote_collector.SetSwapSemiSpaces(false);
    zygote_collector.Run(kGcCauseCollectorTransition, false);
    if (reset_main_space) {
      main_space_->GetMemMap()->Protect(PROT_READ | PROT_WRITE);
      madvise(main_space_->Begin(), main_space_->Capacity(), MADV_DONTNEED);
      MemMap* mem_map = main_space_->ReleaseMemMap();
      RemoveSpace(main_space_);
      space::Space* old_main_space = main_space_;
      CreateMainMallocSpace(mem_map, kDefaultInitialSize, std::min(mem_map->Size(), growth_limit_),mem_map->Size());
      delete old_main_space;
      AddSpace(main_space_);
    } else {
      if (collector_type_ == kCollectorTypeCC) {
        region_space_->GetMemMap()->Protect(PROT_READ | PROT_WRITE);
        // Evacuated everything out of the region space, clear the mark bitmap.
        region_space_->GetMarkBitmap()->Clear();
      } else {
        bump_pointer_space_->GetMemMap()->Protect(PROT_READ | PROT_WRITE);
      }
    }
    if (temp_space_ != nullptr) {
      CHECK(temp_space_->IsEmpty());
    }
    total_objects_freed_ever_ += GetCurrentGcIteration()->GetFreedObjects();
    total_bytes_freed_ever_ += GetCurrentGcIteration()->GetFreedBytes();
    // Update the end and write out image.
    non_moving_space_->SetEnd(target_space.End());
    non_moving_space_->SetLimit(target_space.Limit());
    VLOG(heap) << "Create zygote space with size=" << non_moving_space_->Size() << " bytes";
  }
  // Change the collector to the post zygote one.
  ChangeCollector(foreground_collector_type_);
  // Save the old space so that we can remove it after we complete creating the zygote space.
  space::MallocSpace* old_alloc_space = non_moving_space_;
  // Turn the current alloc space into a zygote space and obtain the new alloc space composed of
  // the remaining available space.
  // Remove the old space before creating the zygote space since creating the zygote space sets
  // the old alloc space's bitmaps to null.
  RemoveSpace(old_alloc_space);
  if (collector::SemiSpace::kUseRememberedSet) {
    // Sanity bound check.
    FindRememberedSetFromSpace(old_alloc_space)->AssertAllDirtyCardsAreWithinSpace();
    // Remove the remembered set for the now zygote space (the old
    // non-moving space). Note now that we have compacted objects into
    // the zygote space, the data in the remembered set is no longer
    // needed. The zygote space will instead have a mod-union table
    // from this point on.
    RemoveRememberedSet(old_alloc_space);
  }
  // Remaining space becomes the new non moving space.
  zygote_space_ = old_alloc_space->CreateZygoteSpace(kNonMovingSpaceName, low_memory_mode_, &non_moving_space_);
  CHECK(!non_moving_space_->CanMoveObjects());
  if (same_space) {
    main_space_ = non_moving_space_;
    SetSpaceAsDefault(main_space_);
  }
  delete old_alloc_space;
  CHECK(HasZygoteSpace()) << "Failed creating zygote space";
  AddSpace(zygote_space_);
  non_moving_space_->SetFootprintLimit(non_moving_space_->Capacity());
  AddSpace(non_moving_space_);
  if (kUseBakerReadBarrier && gc::collector::ConcurrentCopying::kGrayDirtyImmuneObjects) {
    // Treat all of the objects in the zygote as marked to avoid unnecessary dirty pages. This is
    // safe since we mark all of the objects that may reference non immune objects as gray.
    zygote_space_->GetLiveBitmap()->VisitMarkedRange(
        reinterpret_cast<uintptr_t>(zygote_space_->Begin()),
        reinterpret_cast<uintptr_t>(zygote_space_->Limit()),
        [](mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_) {
      CHECK(obj->AtomicSetMarkBit(0, 1));
    });
  }

  // Create the zygote space mod union table.
  accounting::ModUnionTable* mod_union_table =
      new accounting::ModUnionTableCardCache("zygote space mod-union table", this, zygote_space_);
  CHECK(mod_union_table != nullptr) << "Failed to create zygote space mod-union table";

  if (collector_type_ != kCollectorTypeCC) {
    // Set all the cards in the mod-union table since we don't know which objects contain references
    // to large objects.
    mod_union_table->SetCards();
  } else {
    // Make sure to clear the zygote space cards so that we don't dirty pages in the next GC. There
    // may be dirty cards from the zygote compaction or reference processing. These cards are not
    // necessary to have marked since the zygote space may not refer to any objects not in the
    // zygote or image spaces at this point.
    mod_union_table->ProcessCards();
    mod_union_table->ClearTable();

    // For CC we never collect zygote large objects. This means we do not need to set the cards for
    // the zygote mod-union table and we can also clear all of the existing image mod-union tables.
    // The existing mod-union tables are only for image spaces and may only reference zygote and
    // image objects.
    for (auto& pair : mod_union_tables_) {
      CHECK(pair.first->IsImageSpace());
      CHECK(!pair.first->AsImageSpace()->GetImageHeader().IsAppImage());
      accounting::ModUnionTable* table = pair.second;
      table->ClearTable();
    }
  }
  AddModUnionTable(mod_union_table);
  large_object_space_->SetAllLargeObjectsAsZygoteObjects(self);
  if (collector::SemiSpace::kUseRememberedSet) {
    // Add a new remembered set for the post-zygote non-moving space.
    accounting::RememberedSet* post_zygote_non_moving_space_rem_set =
        new accounting::RememberedSet("Post-zygote non-moving space remembered set", this,
                                      non_moving_space_);
    CHECK(post_zygote_non_moving_space_rem_set != nullptr)
        << "Failed to create post-zygote non-moving space remembered set";
    AddRememberedSet(post_zygote_non_moving_space_rem_set);
  }
}